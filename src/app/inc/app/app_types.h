#ifndef _APP_TYPES_H
#define _APP_TYPES_H

#include <stdint.h>

typedef struct {
    const char *dev_serial;
    const char *dev_model;
} app_params_t;

extern app_params_t app_params;

#endif
