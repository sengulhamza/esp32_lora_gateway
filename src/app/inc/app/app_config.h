#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

/* app device type strings */
#define APP_DEVICE_TYPE_MASTER_STR      "master"
#define APP_DEVICE_TYPE_CLIENT_STR      "client"

/* app wifi parameters*/
#define APP_CONFIG_WIFI_SSID            "Blanc Coffee&Cocktails"
#define APP_CONFIG_WIFI_PASS            "blanccoffee2023"

/* app mqtt parametes*/
#define APP_CONFIG_MQTT_BROKER          "mqtt://mqtt.meplis.dev"
#define APP_CONFIG_MQTT_BROKER_PORT     (1883)


/* file paths */
#define APP_CONFIG_FILE_BASE_PATH       "/fs"
#define APP_CONFIG_FILE_APPROVE_GW      APP_CONFIG_FILE_BASE_PATH"/approved_gw.data"
#define APP_CONFIG_FILE_DEVICE_CFG      APP_CONFIG_FILE_BASE_PATH"/device_cfg.json"
#define APP_CONFIG_FILE_DEVICE_CFG_TEMP APP_CONFIG_FILE_BASE_PATH"/device_cfg_temp.json"

/* app configuration parameters */
#define APP_DEV_MODEL                   "MEPLGW"
#define APP_SERIAL                      "12345678"


#endif
