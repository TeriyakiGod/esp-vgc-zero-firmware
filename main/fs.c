#include "fs.h"
#include <stdio.h>
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "FS";

esp_err_t vgc_fs_init()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Mount configuration
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};

    // Mount FAT filesystem with wear leveling
    err = esp_vfs_fat_spiflash_mount_rw_wl("/spiflash", "storage", &mount_config, &vgc_fs_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t vgc_fs_deinit()
{
    // Unmount FAT filesystem
    esp_err_t err = esp_vfs_fat_spiflash_unmount("/spiflash", vgc_fs_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS (%s)", esp_err_to_name(err));
        return err;
    }

    // Deinitialize NVS
    nvs_flash_deinit();

    return ESP_OK;
}
