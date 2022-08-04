#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "core/core_tasks.h"
#include "core/sx127x.h"
#include "app/lora_manager.h"
#include "app/provisioning_manager.h"

#define TEST_APP_KEY "1234567890abcdef"
static const char *TAG = "lora_manager";

lora_tx_packet tx_test_packet = {0};

void lora_prepare_provisioning_packet(lora_tx_packet *packet)
{
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    ESP_ERROR_CHECK(err);
    provisioning_t provisioning_packet = {
        .global_dev_eui = {mac[5], mac[4],  mac[3], 0xfe, 0xff,  mac[2],  mac[1],  mac[0] },
        .app_key = {TEST_APP_KEY},
    };

    packet->packet_id = LORA_PACKET_ID_PROVISING;
    memcpy(packet->data, &provisioning_packet, sizeof(provisioning_packet));
    packet->data_len = sizeof(provisioning_packet);
    packet->end_of_frame = 0xDE;
}

void lora_process_task_tx(void *p)
{
    ESP_LOGI(TAG, "%s started", __func__);
    uint32_t pkg_cnt = 0;
    lora_prepare_provisioning_packet(&tx_test_packet);
    while (pdTRUE) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        sx127x_send_packet((uint8_t *)&tx_test_packet, sizeof(tx_test_packet));
        ESP_LOGI(TAG, "packet sent %d", pkg_cnt);
        pkg_cnt++;
    }
}

void lora_process_task_rx(void *pvParameter)
{
    while (pdTRUE) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(TAG, "%s handled.", __func__);
        if (sx127x_received()) {
            lora_tx_packet lora_rx_packet;
            sx127x_receive_packet((uint8_t *)&lora_rx_packet, sizeof(lora_rx_packet));
            ESP_LOG_BUFFER_HEXDUMP(TAG, &lora_rx_packet, sizeof(lora_rx_packet), ESP_LOG_INFO);
            provisioning_mngr_add_new_client(&lora_rx_packet, TEST_APP_KEY);
            /*
            cJSON *root = cJSON_ParseWithLength((char *)lora_rx_packet.data, lora_rx_packet.data_len);
            if (!root) {
                ESP_LOGE(TAG, "string couldn't be parsed at line (%d)", __LINE__);
            }
            cJSON_Delete(root);
            ESP_LOGI(TAG, "Lora rx packet data:%s - RSSI:%d", lora_rx_packet.data, sx127x_packet_rssi());
            */
        }
    }
}

esp_err_t lora_process_start(void)
{
    esp_err_t ret = ESP_OK;
    sx127x_configure_pins(TTN_SPI_HOST, TTN_PIN_SPI_MISO, TTN_PIN_SPI_MOSI, TTN_PIN_SPI_SCLK, TTN_PIN_NSS, TTN_PIN_RST, TTN_PIN_DIO0);
    sx127x_set_task_params(lora_process_task_rx);

    sx127x_init();

    ret |= xTaskCreate(lora_process_task_tx,
                       "tx_task",
                       CORE_LORA_TASK_STACK,
                       NULL,
                       3,
                       NULL);
    return ret;
}
