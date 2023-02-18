#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "app/wifi_mngr.h"

#define WIFI_MNGR_START_TIMEOUT              3000
#define WIFI_MNGR_DISCONNECT_TIMEOUT         3000
#define WIFI_MNGR_CONNECTED_BIT              BIT0
#define WIFI_MNGR_DISCONNECTED_BIT           BIT1
#define WIFI_MNGR_STARTED_BIT                BIT2
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
static const char *TAG = "wifi-mngr";
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_p_netif = NULL;
static esp_event_handler_instance_t s_wifi_event_any_id = NULL;
static esp_event_handler_instance_t s_ip_event_any_id = NULL;
static char s_ip_addr[16] = {0};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        const wifi_event_t event_type = (wifi_event_t)(event_id);
        switch (event_type) {
        case WIFI_EVENT_STA_START:
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "%s:%d WIFI_EVENT_START", __func__, __LINE__);
            xEventGroupSetBits(s_wifi_event_group, WIFI_MNGR_STARTED_BIT);
            break;
        case WIFI_EVENT_STA_STOP:
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "%s:%d WIFI_EVENT_STOP", __func__, __LINE__);
            break;
        case WIFI_EVENT_STA_CONNECTED:
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "%s:%d WIFI_EVENT_CONNECTED", __func__, __LINE__);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "%s:%d WIFI_EVENT_DISCONNECTED", __func__, __LINE__);
            xEventGroupSetBits(s_wifi_event_group, WIFI_MNGR_DISCONNECTED_BIT);
            break;
        default:
            ESP_LOGW(TAG, "%s:%d Default switch case %" PRIu32 "", __func__, __LINE__, event_id);
            break;
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        const wifi_event_t event_type = (wifi_event_t)(event_id);

        ESP_LOGI(TAG, "%s:%d Event ID %" PRIu32 "", __func__, __LINE__, event_id);

        switch (event_type) {
        case IP_EVENT_STA_GOT_IP: {
            esp_netif_dns_info_t dns_info;
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            ESP_LOGI(TAG, "GOT ip event!!!");
            ESP_LOGI(TAG, "Wifi Connect to the Access Point");
            ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
            ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            esp_netif_get_dns_info(netif, 0, &dns_info);
            ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            esp_netif_get_dns_info(netif, 1, &dns_info);
            ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
            esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_addr, 16);
            xEventGroupSetBits(s_wifi_event_group, WIFI_MNGR_CONNECTED_BIT);
            ESP_LOGI(TAG, "%s:%d CONNECTED!", __func__, __LINE__);
            break;
        }
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "%s:%d IP_LOST.WAITING_FOR_NEW_IP", __func__, __LINE__);
            break;
        case IP_EVENT_GOT_IP6: {
            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_MNGR_CONNECTED_BIT);
            ESP_LOGI(TAG, "%s:%d CONNECTED!", __func__, __LINE__);
            break;
        }
        default:
            ESP_LOGW(TAG, "%s:%d Default switch case %" PRIu32 "", __func__, __LINE__, event_id);
            break;
        }
    }
}

esp_err_t wifi_mngr_connect_sta(uint8_t max_retry)
{
    for (size_t i = 0; i < max_retry; i++) {

        esp_err_t status = esp_wifi_connect();

        if (status != ESP_OK) {
            ESP_LOGE(TAG, "%s:%d wifi connect failed! (%s)",
                     __func__, __LINE__, esp_err_to_name(status));
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_MNGR_CONNECTED_BIT | WIFI_MNGR_DISCONNECTED_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & WIFI_MNGR_CONNECTED_BIT) {
            ESP_LOGI(TAG, "wifi connected to ap");
            return ESP_OK;
        } else if (bits & WIFI_MNGR_DISCONNECTED_BIT) {
            ESP_LOGE(TAG, "Wifi is not connected! retry(%d)", i + 1);
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT %" PRIu32 "", bits);
        }
    }

    return ESP_FAIL;
}

esp_err_t wifi_mngr_connect(const char *ssid, const char *pass)
{
    esp_err_t status = ESP_FAIL;
    ESP_LOGI(TAG, "%s started!", __func__);

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) {
            ESP_LOGE(TAG, "%s:%d event group create error!", __func__, __LINE__);
            return ESP_FAIL;
        }
    }

    if (!s_p_netif) {
        s_p_netif = esp_netif_create_default_wifi_sta();
        if (!s_p_netif) {
            ESP_LOGE(TAG, "%s:%d esp_netif_create error!", __func__, __LINE__);
            return ESP_FAIL;
        }
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    status = esp_wifi_init(&cfg);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "%s:%d esp_wifi_init error!", __func__, __LINE__);
        return ESP_FAIL;
    }

    if (!s_wifi_event_any_id) {
        status = esp_event_handler_instance_register(WIFI_EVENT,
                 ESP_EVENT_ANY_ID,
                 &wifi_event_handler,
                 NULL,
                 &s_wifi_event_any_id);
        if (status != ESP_OK) {
            ESP_LOGE(TAG, "%s:%d wifi event register error!", __func__, __LINE__);
            return ESP_FAIL;
        }
    }

    if (!s_ip_event_any_id) {
        status = esp_event_handler_instance_register(IP_EVENT,
                 ESP_EVENT_ANY_ID,
                 &ip_event_handler,
                 NULL,
                 &s_ip_event_any_id);
        if (status != ESP_OK) {
            ESP_LOGE(TAG, "%s:%d ip event register error!", __func__, __LINE__);
            return ESP_FAIL;
        }
    }

    status = esp_wifi_set_mode(WIFI_MODE_STA);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "%s:%d wifi set mode error!", __func__, __LINE__);
        return ESP_FAIL;
    }

    static wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, ssid,
           MIN(strlen(ssid), sizeof(wifi_config.sta.ssid)));

    memcpy(wifi_config.sta.password, pass,
           MIN(strlen(pass), sizeof(wifi_config.sta.password)));

    wifi_config.sta.threshold.authmode  = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable     = true;
    wifi_config.sta.pmf_cfg.required    = false;

    status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "%s:%d wifi set config error!", __func__, __LINE__);
        return ESP_FAIL;
    }

    status = esp_wifi_start();
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "%s:%d wifi start error!", __func__, __LINE__);
        return ESP_FAIL;
    }
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_MNGR_STARTED_BIT,
                                           pdTRUE,
                                           pdTRUE,
                                           pdMS_TO_TICKS(WIFI_MNGR_START_TIMEOUT));

    if (bits & WIFI_MNGR_STARTED_BIT) {
        ESP_LOGI(TAG, "%s:%d Wifi started successfully", __func__, __LINE__);
        return wifi_mngr_connect_sta(10);
    }

    ESP_LOGE(TAG, "%s:%d unknown event or timeout!", __func__, __LINE__);

    return ESP_FAIL;
}

wifi_states_t wifi_mngr_state(void)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_MNGR_CONNECTED_BIT | WIFI_MNGR_DISCONNECTED_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           0);

    if (bits & WIFI_MNGR_CONNECTED_BIT) {
        return WIFI_MNGR_CONNECTED;
    } else if (bits & WIFI_MNGR_DISCONNECTED_BIT) {
        return WIFI_MNGR_DISCONNECTED;
    } else {
        if (bits != 0) {
            ESP_LOGE(TAG, "UNEXPECTED EVENT %" PRIu32 "", bits);
            return WIFI_MNGR_ERROR;
        }
    }

    return WIFI_MNGR_STATUS_NOT_CHANGED;
}
