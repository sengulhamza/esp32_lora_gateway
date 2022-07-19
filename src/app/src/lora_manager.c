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

static const char *TAG = "lora_manager";

void lora_process_task_tx(void *p)
{
    char *dummy = " Hello lora band world!";
    ESP_LOGI(TAG, "%s started", __func__);
    uint32_t pkg_cnt = 0;
    while (pdTRUE) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        char buff[256] = {0};
        sprintf(buff, "%sPacket:%d", dummy, pkg_cnt);
        sx127x_send_packet((uint8_t *) buff, strlen(buff));
        ESP_LOGI(TAG, "packet sent: %s", buff);
        pkg_cnt++;
    }
}
#if 1
void lora_process_task_rx(void *p)
{
    while (pdTRUE) {
        sx127x_receive();
        if (sx127x_received()) {
            uint8_t buf[256] = {0};
            sx127x_receive_packet(buf, sizeof(typeof(buf)));
            ESP_LOGI(TAG, "Recieved packet:%s", (char *)buf);
            lora_tx_packet lora_rx_packet;
            memcpy((uint8_t *)&lora_rx_packet, buf, sizeof(lora_tx_packet));
            cJSON *root = cJSON_ParseWithLength(lora_rx_packet.data, lora_rx_packet.data_len);
            if (!root) {
                ESP_LOGE(TAG, "mqtt string couldn't be parsed at line (%d)", __LINE__);
                goto _exit;
            }

            uint8_t node_id = 0;
            cJSON *node_num = cJSON_GetObjectItemCaseSensitive(root, "node_id");
            if (cJSON_IsNumber(node_num) && node_num->valueint) {
                node_id = node_num->valueint;
                ESP_LOGI(TAG, "Node Num:%d", node_id);
            }

            node_num = cJSON_GetObjectItemCaseSensitive(root, "poll_ms");
            if (cJSON_IsNumber(node_num) && node_num->valueint) {
                uint32_t poll_ms = node_num->valueint;
                ESP_LOGI(TAG, "Poll ms:%d", poll_ms);
            }
_exit:      cJSON_Delete(root);
            sx127x_receive();
            printf("Received:%s - RSSI:%d\n", lora_rx_packet.data, sx127x_packet_rssi());
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif

void lmic_background_task(void *pvParameter)
{
    ESP_LOGW(TAG, "%s started!", __func__);
    while (pdTRUE) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(TAG, "%s handled.", __func__);
    }
    vTaskDelete(NULL);
}

esp_err_t lora_process_start(void)
{
    esp_err_t ret = ESP_OK;
    sx127x_configure_pins(TTN_SPI_HOST, TTN_PIN_SPI_MISO, TTN_PIN_SPI_MOSI, TTN_PIN_SPI_SCLK, TTN_PIN_NSS, TTN_PIN_RST, TTN_PIN_DIO0);
    sx127x_set_task_params(lmic_background_task);

    sx127x_init();

    ret |= xTaskCreate(lora_process_task_rx,
                       CORE_LORA_TASK_NAME,
                       CORE_LORA_TASK_STACK,
                       NULL,
                       CORE_LORA_TASK_PRIO,
                       NULL);

    ret |= xTaskCreate(lora_process_task_tx,
                       "tx_task",
                       CORE_LORA_TASK_STACK,
                       NULL,
                       3,
                       NULL);
    return ret;


}
