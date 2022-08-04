#ifndef _PROVISIONING_MANAGER_H_
#define _PROVISIONING_MANAGER_H_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t global_dev_eui[8];
    uint8_t app_key[16];
} provisioning_t;

esp_err_t provisioning_mngr_add_new_client(lora_tx_packet *lora_data, char *app_key);

#ifdef __cplusplus
}
#endif

#endif
