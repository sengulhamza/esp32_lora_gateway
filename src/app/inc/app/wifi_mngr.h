#ifndef _WIFI_MNGR_H_
#define _WIFI_MNGR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    WIFI_MNGR_CONNECTED,
    WIFI_MNGR_DISCONNECTED,
    WIFI_MNGR_STATUS_NOT_CHANGED,
    WIFI_MNGR_ERROR
} wifi_states_t;

esp_err_t wifi_mngr_connect(const char *ssid, const char *pass);
wifi_states_t wifi_mngr_state(void);

#ifdef __cplusplus
}
#endif

#endif