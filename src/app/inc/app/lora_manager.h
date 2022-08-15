#ifndef _LORA_MANAGER_H_
#define _LORA_MANAGER_H_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_PACKET_ID_PROVISING 0xE1
#define LORA_PACKET_ID_PROVISING_OK 0xD1

typedef struct {
    uint8_t packet_id;
    uint8_t data[58];
    uint16_t data_len;
    uint8_t end_of_frame;
} lora_tx_packet;

esp_err_t lora_process_start(void);
esp_err_t lora_send_tx_queue(uint8_t packet_id, uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif
