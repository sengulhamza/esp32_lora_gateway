#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_err.h"
#include "esp_log.h"

char *utils_get_mac(void)
{
    static char s_mac_addr_cstr[12 + 1] = {0};

    if (s_mac_addr_cstr[0] == '\0') {

        uint8_t mac_byte_buffer[6] = {0};   ///< Buffer to hold MAC as bytes

        // Get the MAC as bytes from the ESP API
        const esp_err_t status = esp_efuse_mac_get_default(mac_byte_buffer);

        if (status == ESP_OK) {
            // Convert the bytes to a cstring and store
            //   in our static buffer as ASCII HEX
            snprintf(s_mac_addr_cstr, sizeof(s_mac_addr_cstr),
                     "%02X%02X%02X%02X%02X%02X",
                     mac_byte_buffer[0],
                     mac_byte_buffer[1],
                     mac_byte_buffer[2],
                     mac_byte_buffer[3],
                     mac_byte_buffer[4],
                     mac_byte_buffer[5]);
        }
    }

    return s_mac_addr_cstr;
}