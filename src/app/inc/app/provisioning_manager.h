#ifndef _PROVISIONING_MANAGER_H_
#define _PROVISIONING_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t global_dev_eui[12 + 1];
    uint8_t app_key[16];
} provisioning_t;

esp_err_t provisioning_mngr_add_new_client(lora_frame_t *lora_data, char *app_key);
esp_err_t provisioning_mngr_provis_is_ok(lora_frame_t *lora_data, char *app_key);
bool provisioning_mngr_check_device_is_approved(void);


#ifdef __cplusplus
}
#endif

#endif
