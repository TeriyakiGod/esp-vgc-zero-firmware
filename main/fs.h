#ifndef BITSYBOX_H
#define BITSYBOX_H

#include "esp_err.h"
#include "esp_vfs_fat.h"

/* Wear level handle */
static wl_handle_t vgc_fs_wl_handle;

esp_err_t vgc_fs_init();
esp_err_t vgc_fs_deinit();

#endif // BITSYBOX_H