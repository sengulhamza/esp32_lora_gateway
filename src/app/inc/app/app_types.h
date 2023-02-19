#ifndef _APP_TYPES_H
#define _APP_TYPES_H

#include <stdint.h>

typedef enum {
    APP_DEVICE_IS_MASTER,
    APP_DEVICE_IS_CLIENT
} app_device_type;

typedef struct {
    const char *dev_serial;
    const char *dev_model;
    const char *dev_wifi_ssid;
    const char *dev_wifi_pass;
    const char *dev_mqtt_broker;
    uint32_t dev_mqtt_broker_port;
    app_device_type device_type;
} app_params_t;

extern app_params_t app_params;

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   (sizeof((a)) / sizeof((a)[0]))
#endif

#endif
