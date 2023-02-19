#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "app/app_types.h"
#include "app/app_config.h"
#include "app/mqtt_mngr.h"

static const char *TAG = "mqtt-mngr";
typedef struct {
    int msg_id;
    const char *topic;
    bool subscribed;
} mqtt_sub_table_t;

static esp_mqtt_client_handle_t s_mqtt_client;
static uint8_t s_mqtt_disconnected_cnt = 0;
static bool s_mqtt_connected = false;
static uint8_t s_max_retry_before_reconfig = 5; /* default val */
static void (*f_event_cb)(const char *, esp_mqtt_event_handle_t);

static mqtt_sub_table_t s_mqtt_sub_table[] = {
    { .msg_id = -1, MQTT_CONFIG_TOPIC, false},
};

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    static int8_t s_latest_topic_index = -1;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        s_mqtt_disconnected_cnt = 0;
        s_mqtt_connected = true;
        for (size_t i = 0; i < ARRAY_SIZE(s_mqtt_sub_table); i++) {
            s_mqtt_sub_table[i].msg_id = esp_mqtt_client_subscribe(event->client, s_mqtt_sub_table[i].topic, 0);
            s_mqtt_sub_table[i].subscribed = true;
            ESP_LOGI(TAG, "%s subscribe res msg_id= %d",
                     s_mqtt_sub_table[i].topic,
                     s_mqtt_sub_table[i].msg_id);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED (%d)", s_mqtt_disconnected_cnt + 1);
        s_mqtt_connected = false;
        ++s_mqtt_disconnected_cnt;
        if (s_mqtt_disconnected_cnt > s_max_retry_before_reconfig) {
            ESP_LOGE(TAG, "Mqtt connect attempts failed.Need to check connection from another server!");
        }
        for (size_t i = 0; i < ARRAY_SIZE(s_mqtt_sub_table); i++) {
            s_mqtt_sub_table[i].subscribed = false;
        }
        ESP_LOGW(TAG, "free_heap/min_heap size %" PRIu32 "/%" PRIu32 " Bytes",
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size());
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        for (size_t i = 0; i < ARRAY_SIZE(s_mqtt_sub_table); i++) {
            if (s_mqtt_sub_table[i].msg_id == event->msg_id) {
                s_mqtt_sub_table[i].subscribed = true;
                break;
            }
        }
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        for (size_t i = 0; i < ARRAY_SIZE(s_mqtt_sub_table); i++) {
            if (s_mqtt_sub_table[i].msg_id == event->msg_id) {
                s_mqtt_sub_table[i].subscribed = false;
                break;
            }
        }
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA msg_id:%d %d/%d: %d %d",
                 event->msg_id,
                 event->current_data_offset, event->total_data_len,
                 event->topic_len, event->data_len
                );

        /* in multiple event data only first event contains topic
            pointer and length
        */
        if (event->topic_len == 0 && s_latest_topic_index >= 0) {
            if (f_event_cb) {
                f_event_cb(s_mqtt_sub_table[s_latest_topic_index].topic, event);
            }
        } else {
            for (size_t i = 0; i < ARRAY_SIZE(s_mqtt_sub_table); i++) {
                if (s_mqtt_sub_table[i].subscribed &&
                        !strncmp(event->topic, s_mqtt_sub_table[i].topic, event->topic_len)) {
                    s_latest_topic_index = i;
                    if (f_event_cb) {
                        f_event_cb(s_mqtt_sub_table[i].topic, event);
                    }
                }
            }
        }
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

bool mqtt_is_connected(void)
{
    return s_mqtt_connected;
}

esp_err_t mqtt_publish_data(const char *topic, const char *data)
{
    if (!s_mqtt_connected || !topic || !data) {
        ESP_LOGE(TAG, "s_mqtt_disconnected_cnt: (%d)", s_mqtt_disconnected_cnt);
        return ESP_FAIL;
    }
    int res = esp_mqtt_client_publish(s_mqtt_client, topic, data, strlen(data), 0, 0);
    ESP_LOGI(TAG, "publish to %s res: %d", topic, res);

    return res == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_process_start_client(const char *broker, uint32_t port, const char *uname, const char *pass, void *event_data_callback)
{
    if (!broker) {
        ESP_LOGE(TAG, "mqtt broker is null!");
        return ESP_FAIL;
    }

    esp_mqtt_client_config_t mqtt_cfg = {0};

    mqtt_cfg.broker.address.uri = strdup(broker);
    mqtt_cfg.broker.address.port = port;

    if (uname && pass) {
        if (mqtt_cfg.credentials.username) {
            mqtt_cfg.credentials.username = strdup(uname);
        }
        mqtt_cfg.credentials.authentication.password = strdup(pass);
    }
    mqtt_cfg.credentials.client_id = app_params.dev_serial;
    mqtt_cfg.network.reconnect_timeout_ms = 5000;
    mqtt_cfg.network.timeout_ms = 5000;
    mqtt_cfg.session.disable_keepalive = true;
    f_event_cb = event_data_callback;

    ESP_LOGI(TAG, "mqtt client connecting to %s", mqtt_cfg.broker.address.uri);

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "mqtt client init failed!");
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
    return ESP_OK;
}
