#ifndef FS_H
#define FS_H

#include "esp_err.h"
#include "esp_vfs_fat.h"

esp_err_t vgc_fs_init();
esp_err_t vgc_fs_deinit();

#endif // FS_H