#ifndef _MQTT_MNGR_H_
#define _MQTT_MNGR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_CONFIG_TOPIC               "device/cfg"

#include <stdint.h>
#include "esp_err.h"

esp_err_t mqtt_process_start_client(const char *broker, uint32_t port, const char *uname, const char *pass, void *event_data_callback);

#ifdef __cplusplus
}
#endif

#endif