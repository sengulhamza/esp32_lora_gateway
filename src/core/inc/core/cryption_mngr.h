#ifndef _CRYPTION_MNGR_H_
#define _CRYPTION_MNGR_H_

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    CRYPTION_FAIL = false,
    CRYPTION_OK = true
} cryption_status;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cryption_mngr_init(char *key);
esp_err_t cryption_mngr_decrypt(char *input, size_t len, char *output);
esp_err_t cryption_mngr_encrypt(char *input, size_t len, char *output);

#ifdef __cplusplus
}
#endif

#endif
