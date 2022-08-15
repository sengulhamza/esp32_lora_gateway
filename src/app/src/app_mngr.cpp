#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "core_includes.h"
#include "core/sx127x.h"
#include "core/utils.h"
#include "app_config.h"
#include "app_types.h"
#include "app/lora_manager.h"

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

    ESP_LOGI(TAG, "%s %s has started from @%x",
             app_desc.project_name,
             app_desc.version,
             running->address
            );

    ESP_LOGI(TAG, "serial: %s", app_params.dev_serial);
}

static void print_heap_usage(const char *msg)
{
    ESP_LOGW(TAG, "(%s):free_heap/min_heap size %d/%d Bytes",
             msg,
             esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());
}

#endif

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
    status |= lora_process_start();
    uint8_t version;
    uint8_t i = 0;
    while (i++ < 100) {
        version = sx127x_read_reg(0x42);
        ESP_LOGI(TAG, "version is:0x%x", version);
        if (version == 0x12) {
            break;
        }
        vTaskDelay(2);
    }
    ESP_LOGI(TAG, "first init done... status: %d", status);

#ifdef DEBUG_BUILD
    print_heap_usage("after first init");
#endif

    return ESP_OK;
}
