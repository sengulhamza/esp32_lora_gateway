#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "core/core_tasks.h"
#include "core/sx127x.h"
#include "core/utils.h"
#include "app/app_types.h"
#include "app/lora_manager.h"
#include "app/provisioning_manager.h"

#define TEST_APP_KEY "1234567890abcdef"
#define LORA_TX_QUEUE_SIZE 10

static const char *TAG = "lora_manager";

static xQueueHandle s_tx_queue = {0};

lora_tx_packet tx_test_packet = {0};

void lora_prepare_provisioning_packet(lora_tx_packet *packet)
{
    provisioning_t provisioning_packet = {
        .app_key = {TEST_APP_KEY},
    };
    strcpy((char *)provisioning_packet.global_dev_eui,  utils_get_mac());
    packet->packet_id = LORA_PACKET_ID_PROVISING;
    memcpy(packet->data, &provisioning_packet, sizeof(provisioning_packet));
    packet->data_len = sizeof(provisioning_packet);
    packet->end_of_frame = 0xDE;
}

esp_err_t lora_send_tx_queue(uint8_t packet_id, uint8_t *data, uint8_t data_len)
{
    lora_tx_packet tx_queue_packet = {0};
    if (packet_id == LORA_PACKET_ID_PROVISING_OK) {
        lora_prepare_provisioning_packet(&tx_queue_packet);
        tx_queue_packet.packet_id = LORA_PACKET_ID_PROVISING_OK;
        ESP_LOGW(TAG, "%s handled", __func__);
    } else {
        tx_queue_packet.packet_id = packet_id;
        if (data != NULL) {
            memcpy(&tx_queue_packet.data, data, data_len);
        }
        if (data_len) {
            tx_queue_packet.data_len = data_len;
        }
        tx_queue_packet.end_of_frame = 0xDE;
    }
    if (xQueueSend(s_tx_queue, (void *)&tx_queue_packet, 0) == pdPASS) {
        ESP_LOGI(TAG, "Lora tx command processed, waiting msg cnt:%d", uxQueueMessagesWaiting(s_tx_queue));
        return ESP_OK;
    }
    return ESP_FAIL;
}

void lora_process_task_tx(void *p)
{
    ESP_LOGI(TAG, "%s started", __func__);

    uint8_t hello_lora[] = {"Hello lora devices!"};
    sx127x_send_packet(hello_lora, sizeof(hello_lora));

    /* TODO Device mod paramater will update before all task in app_mngr. */
    bool device_is_master_test = false;
    if (!strcmp(app_params.dev_serial, "MEPLGW7821848D25D4")) {
        ESP_LOGW(TAG, "MASTER DEVICE!");
        device_is_master_test = true;
        goto lora_tx_loop;
    }

    while (!provisioning_mngr_check_device_is_approved()) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        lora_prepare_provisioning_packet(&tx_test_packet);
        sx127x_send_packet((uint8_t *)&tx_test_packet, sizeof(tx_test_packet));
        ESP_LOGI(TAG, "provisioning packet sent");
    }

lora_tx_loop:
    while (pdTRUE) {
        /* check if there is something to write */
        if (uxQueueMessagesWaiting(s_tx_queue) > 0) {
            lora_tx_packet tx_packet = {0};
            if (xQueueReceive(s_tx_queue, (void *)&tx_packet, 0)) {
                sx127x_send_packet((uint8_t *)&tx_packet, sizeof(tx_packet));
                ESP_LOGI(TAG, "there is a tx request. ID: %x", tx_packet.packet_id);
            }
        } else if (!device_is_master_test) {
            uint8_t test_data[] = {"0123456789ABCDEF_client_test_data"};
            lora_send_tx_queue(0xAE, test_data, sizeof(test_data));
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void lora_rx_commander(lora_tx_packet *lora_rx_packet)
{
    ESP_LOGI(TAG, "%s handled", __func__);
    switch (lora_rx_packet->packet_id) {
    case LORA_PACKET_ID_PROVISING:
        provisioning_mngr_add_new_client(lora_rx_packet, TEST_APP_KEY);
        break;
    case LORA_PACKET_ID_PROVISING_OK:
        provisioning_mngr_provis_is_ok(lora_rx_packet, TEST_APP_KEY);
        break;
    default:
        break;
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
            lora_rx_commander(&lora_rx_packet);
        }
        sx127x_receive();
    }
}

esp_err_t lora_process_start(void)
{
    esp_err_t ret = ESP_OK;
    sx127x_configure_pins(TTN_SPI_HOST, TTN_PIN_SPI_MISO, TTN_PIN_SPI_MOSI, TTN_PIN_SPI_SCLK, TTN_PIN_NSS, TTN_PIN_RST, TTN_PIN_DIO0);
    sx127x_set_task_params(lora_process_task_rx);

    sx127x_init();
    s_tx_queue = xQueueCreate(LORA_TX_QUEUE_SIZE, sizeof(lora_tx_packet));
    if (!s_tx_queue) {
        ESP_LOGE(TAG, "couldn't create the lora tx queue!");
        return ESP_FAIL;
    }
    ret |= xTaskCreate(lora_process_task_tx,
                       CORE_LORA_TASK_NAME,
                       CORE_LORA_TASK_STACK,
                       NULL,
                       CORE_LORA_TASK_PRIO,
                       NULL);
    return ret;
}
