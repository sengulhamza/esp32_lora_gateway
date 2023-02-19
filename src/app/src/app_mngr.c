#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "core_includes.h"
#include "core/sx127x.h"
#include "core/utils.h"
#include "app/app_config.h"
#include "app/app_types.h"
#include "app/wifi_mngr.h"
#include "app/lora_manager.h"
#include "app/mqtt_mngr.h"

static const char *TAG = "appmngr";

app_params_t app_params;

static void app_core_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(file_mngr_init(APP_CONFIG_FILE_BASE_PATH));
}

static char *app_get_serial(void)
{
    static char s_app_serial[32 + 1] = {0};

    if (s_app_serial[0] == '\0') {
        strcpy(s_app_serial, APP_DEV_MODEL);
        strcat(s_app_serial, utils_get_mac());
    }
    return s_app_serial;
}

#ifdef DEBUG_BUILD
static void print_app_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t app_desc;
    ESP_ERROR_CHECK(esp_ota_get_partition_description(running, &app_desc));

    ESP_LOGI(TAG, "%s %s has started from @%" PRIu32 "",
             app_desc.project_name,
             app_desc.version,
             running->address
            );

    ESP_LOGI(TAG, "serial: %s", app_params.dev_serial);
}

static void print_heap_usage(const char *msg)
{
    ESP_LOGW(TAG, "(%s):free_heap/min_heap size %" PRIu32 "/%" PRIu32 " Bytes",
             msg,
             esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());
}

#endif

static esp_err_t app_parse_config_data(char *data, uint16_t data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (!root) {
        ESP_LOGE(TAG, "json string couldn't be parsed at line (%d)", __LINE__);
        return ESP_FAIL;
    }
    cJSON *object  =  cJSON_GetObjectItemCaseSensitive(root, "device_type");
    if (cJSON_IsString(object)) {
        if (!strcmp(object->valuestring, APP_DEVICE_TYPE_MASTER_STR)) {
            app_params.device_type = APP_DEVICE_IS_MASTER;
        } else if (!strcmp(object->valuestring, APP_DEVICE_TYPE_CLIENT_STR)) {
            app_params.device_type = APP_DEVICE_IS_CLIENT;
        } else {
            ESP_LOGW(TAG, "device_type object couldn't match any type(%s). device_type changed to default(%s).!", object->valuestring, APP_DEVICE_TYPE_MASTER_STR);
        }
        ESP_LOGI(TAG, "Device type is %d - %s", app_params.device_type, app_params.device_type ? APP_DEVICE_TYPE_CLIENT_STR : APP_DEVICE_TYPE_MASTER_STR);
    }
    object  =  cJSON_GetObjectItemCaseSensitive(root, "wifi_ssid");
    if (cJSON_IsString(object)) {
        app_params.dev_wifi_ssid = strdup(object->valuestring);
        ESP_LOGI(TAG, "Device wifi ssid is %s", app_params.dev_wifi_ssid);
    }
    object  =  cJSON_GetObjectItemCaseSensitive(root, "wifi_pass");
    if (cJSON_IsString(object)) {
        app_params.dev_wifi_pass = strdup(object->valuestring);
        ESP_LOGI(TAG, "Device wifi pass is %s", app_params.dev_wifi_pass);
    }

    object = cJSON_GetObjectItemCaseSensitive(root, "mqtt_broker");
    if (cJSON_IsString(object)) {
        app_params.dev_mqtt_broker = strdup(object->valuestring);
        ESP_LOGI(TAG, "Device MQTT Broker Url: %s", app_params.dev_mqtt_broker);
    }

    object = cJSON_GetObjectItemCaseSensitive(root, "mqtt_broker_port");
    if (cJSON_IsNumber(object)) {
        app_params.dev_mqtt_broker_port = object->valueint;
        ESP_LOGI(TAG, "Device MQTT Broker Port:%" PRIu32 "", app_params.dev_mqtt_broker_port);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t app_set_default_dev_config(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_type", APP_DEVICE_TYPE_MASTER_STR);
    cJSON_AddStringToObject(root, "wifi_ssid", APP_CONFIG_WIFI_SSID);
    cJSON_AddStringToObject(root, "wifi_pass", APP_CONFIG_WIFI_PASS);
    cJSON_AddStringToObject(root, "mqtt_broker", APP_CONFIG_MQTT_BROKER);
    cJSON_AddNumberToObject(root, "mqtt_broker_port", APP_CONFIG_MQTT_BROKER_PORT);

    const char *ptr = cJSON_PrintUnformatted(root);
    file_overwrite(APP_CONFIG_FILE_DEVICE_CFG, ptr, strlen(ptr));
    cJSON_free((void *)ptr);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t app_get_device_config(void)
{
    char *buff = NULL;
    esp_err_t status = ESP_OK;

    if (!file_is_exist(APP_CONFIG_FILE_DEVICE_CFG)) {
        ESP_LOGE(TAG, "%s not found", APP_CONFIG_FILE_DEVICE_CFG);
        if (app_set_default_dev_config() != ESP_OK) {
            return ESP_FAIL;
        }
    }

    int flen = file_read(APP_CONFIG_FILE_DEVICE_CFG, &buff);
    if (flen > 0) {
        ESP_LOGI(TAG, "%d bytes read from %s", flen, APP_CONFIG_FILE_DEVICE_CFG);
        status = app_parse_config_data(buff, flen);
    }
    free((void *)buff);
    return status;
}

static esp_err_t app_update_configs(void)
{
    char *buff = NULL;
    esp_err_t status = ESP_FAIL;

    if (!file_is_exist(APP_CONFIG_FILE_DEVICE_CFG_TEMP)) {
        ESP_LOGE(TAG, "%s not found", APP_CONFIG_FILE_DEVICE_CFG_TEMP);
        return ESP_FAIL;
    }

    int flen = file_read(APP_CONFIG_FILE_DEVICE_CFG_TEMP, &buff);
    if (flen > 0) {
        ESP_LOGI(TAG, "%d bytes read from %s", flen, APP_CONFIG_FILE_DEVICE_CFG_TEMP);
        file_delete(APP_CONFIG_FILE_DEVICE_CFG);
        status = file_rename(APP_CONFIG_FILE_DEVICE_CFG_TEMP, APP_CONFIG_FILE_DEVICE_CFG);
        ESP_LOGI(TAG, "%s renamed to %s", APP_CONFIG_FILE_DEVICE_CFG_TEMP, APP_CONFIG_FILE_DEVICE_CFG);
    }

    free((void *)buff);
    return status;
}

static void app_mngr_mqtt_sub_handle(const char *topic_name, esp_mqtt_event_handle_t evt)
{
    ESP_LOGI(TAG, "topic:(%s) total:(%d) offset:(%d) len:(%d)",
             topic_name,
             evt->total_data_len,
             evt->current_data_offset,
             evt->data_len);

    if (!strcmp(topic_name, MQTT_CONFIG_TOPIC)) {
        if (evt->current_data_offset == 0) {
            file_delete(APP_CONFIG_FILE_DEVICE_CFG_TEMP);
        }
        file_append(APP_CONFIG_FILE_DEVICE_CFG_TEMP, evt->data, evt->data_len);

        /* topic:(s/cfg) total:(2467) offset:(2038) len:(429) */
        if (evt->current_data_offset + evt->data_len == evt->total_data_len) {
            if (app_update_configs() == ESP_OK) {
                ESP_LOGW(TAG, "Settings updated. App will restart in 1 second!");
                esp_restart();
            }
        }
    }
}

esp_err_t app_start(void)
{
#ifdef DEBUG_BUILD
    esp_log_level_set("*", ESP_LOG_INFO);           // set all components to INFO level
#else
    esp_log_level_set("*", ESP_LOG_NONE);           // disable logs for all components
#endif

    app_core_init();

    app_params.dev_serial = app_get_serial();
    app_params.dev_model = APP_DEV_MODEL;

#ifdef DEBUG_BUILD
    print_app_info();
#endif

    esp_err_t status = ESP_OK;
    status |= app_get_device_config();
    status |= lora_process_start();
    if (app_params.device_type == APP_DEVICE_IS_MASTER) {
        status |= wifi_mngr_connect(app_params.dev_wifi_ssid, app_params.dev_wifi_pass);
        status |= mqtt_process_start_client(app_params.dev_mqtt_broker, app_params.dev_mqtt_broker_port, NULL, NULL, app_mngr_mqtt_sub_handle);
    }
    ESP_LOGI(TAG, "first init done... status: %d", status);

#ifdef DEBUG_BUILD
    print_heap_usage("after first init");
#endif

    return ESP_OK;
}
