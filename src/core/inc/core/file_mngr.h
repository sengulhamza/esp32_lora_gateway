#ifndef _FILE_MNGR_H_
#define _FILE_MNGR_H_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t file_mngr_init(const char *base_path);
bool file_is_exist(const char *path);
int file_delete(const char *path);
int file_size(const char *path);
int file_read(const char *path, char **buff);
int file_write(const char *path, const char *buff, int buff_len);
int file_overwrite(const char *path, const char *buff, int buff_len);
int file_append(const char *path, const char *buff, int buff_len);
esp_err_t file_rename(const char *old_name, const char *new_name);

#ifdef __cplusplus
}
#endif

#endif
