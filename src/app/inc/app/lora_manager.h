#ifndef _LORA_MANAGER_H_
#define _LORA_MANAGER_H_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t node_id;
    char    data[250];
    uint16_t data_len;
} lora_tx_packet;

esp_err_t lora_process_start(void);

#ifdef __cplusplus
}
#endif

#endif
