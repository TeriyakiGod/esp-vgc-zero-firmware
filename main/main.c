#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "duktape.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>

static const char *TAG = "vgc_firmware";

static duk_ret_t duk_print(duk_context *ctx)
{
    int nargs = duk_get_top(ctx); // Get number of arguments
    for (int i = 0; i < nargs; i++)
    {
        if (i > 0)
        {
            ESP_LOGI("Duktape", " "); // Print a space between arguments
        }
        ESP_LOGI("Duktape", "%s", duk_safe_to_string(ctx, i)); // Print each argument
    }
    return 0; // No return value
}

void *duk_psram_alloc(void *udata, duk_size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void *duk_psram_realloc(void *udata, void *ptr, duk_size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
}

void duk_psram_free(void *udata, void *ptr)
{
    heap_caps_free(ptr);
}

static void duk_fatal_error(void *udata, const char *msg)
{
    ESP_LOGE(TAG, "Fatal error: %s", msg);
}

static void log_mem(){
    ESP_LOGI(TAG, "PSRAM left %d KB", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI(TAG, "RAM left %" PRIu32 " KB", (esp_get_free_heap_size() - heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / 1024);
}

typedef enum
{
    FILETYPE_SCRIPT,
    FILETYPE_FONT
} filetype_t;

bool loadScript(duk_context *ctx, const char *filepath, filetype_t type, const char *variableName)
{
    bool success = true;

    ESP_LOGI(TAG, "Loading script/font: %s", filepath);

    char *fileBuffer = 0;
    long length;
    FILE *f = fopen(filepath, "r");

    if (f)
    {
        fseek(f, 0, SEEK_END);

        length = ftell(f);

        fseek(f, 0, SEEK_SET);

        // length +1 to make sure there's room for null terminator
        fileBuffer = heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM);

        if (fileBuffer)
        {
            // replace seek length with read length
            length = fread(fileBuffer, 1, length, f);

            // ensure null terminator
            fileBuffer[length] = '\0';
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for file: %s", filepath);
            success = false;
        }

        fclose(f);
    } else {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        success = false;
    }

    if (fileBuffer)
    {
        // Push the file contents onto the Duktape stack
        duk_push_lstring(ctx, (const char *)fileBuffer, (duk_size_t)length);

        if (type == FILETYPE_SCRIPT)
        {
            // Load the script and check for errors
            if (duk_peval(ctx) != 0)
            {
                ESP_LOGE(TAG, "Load Script Error: %s\n", duk_safe_to_string(ctx, -1));
                success = false;
            }
            else
            {
                success = true;
            }
            // Pop the result off the stack
            duk_pop(ctx);
        }
        else
        {
            // Load the font as a string onto the Duktape stack
            duk_put_global_string(ctx, variableName);
        }
        free(fileBuffer);
    }
    // todo: use duk_compile instead?
    return success;
}

bool duk_load_bitsy_engine(duk_context *ctx)
{
    bool success = true;

    // load engine
    const char *scripts[] = {
        "/spiflash/bitsy/engine/script.js",
        "/spiflash/bitsy/engine/font.js",
        "/spiflash/bitsy/engine/transition.js",
        "/spiflash/bitsy/engine/dialog.js",
        "/spiflash/bitsy/engine/renderer.js",
        "/spiflash/bitsy/engine/bitsy.js"};

    for (int i = 0; i < sizeof(scripts) / sizeof(scripts[0]); i++)
    {
        //log_mem();
        // Load scripts
        if (!loadScript(ctx, scripts[i], FILETYPE_SCRIPT, NULL))
        {
            ESP_LOGE(TAG, "Failed to load script: %s", scripts[i]);
            success = false;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay for 10ms
    }

    // load font
    const char *font_path = "/spiflash/bitsy/font/ascii_small.bitsyfont";
    if (!loadScript(ctx, font_path, FILETYPE_FONT, "__bitsybox_default_font__"))
    {
        ESP_LOGE(TAG, "Failed to load font: %s", font_path);
        success = false;
    }
    return success;
}

duk_ret_t bitsyLog(duk_context *ctx)
{
    const char *printStr;
    printStr = duk_safe_to_string(ctx, 0);

    ESP_LOGI(TAG, "Bitsy: %s", printStr);
    return 0;
}

duk_ret_t bitsyOnLoad(duk_context *ctx)
{
    // hacky to just stick it in the global namespace??
    duk_put_global_string(ctx, "__bitsybox_on_load__");
    return 0;
}

duk_ret_t bitsyOnQuit(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_quit__");
    return 0;
}

duk_ret_t bitsyOnUpdate(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_update__");
    return 0;
}

void register_bitsy_api(duk_context *ctx)
{
    duk_push_c_function(ctx, bitsyLog, 2);
    duk_put_global_string(ctx, "bitsyLog");

    // duk_push_c_function(ctx, bitsyGetButton, 1);
    // duk_put_global_string(ctx, "bitsyGetButton");

    // duk_push_c_function(ctx, bitsySetGraphicsMode, 1);
    // duk_put_global_string(ctx, "bitsySetGraphicsMode");

    // duk_push_c_function(ctx, bitsySetColor, 4);
    // duk_put_global_string(ctx, "bitsySetColor");

    // duk_push_c_function(ctx, bitsyResetColors, 0);
    // duk_put_global_string(ctx, "bitsyResetColors");

    // duk_push_c_function(ctx, bitsyDrawBegin, 1);
    // duk_put_global_string(ctx, "bitsyDrawBegin");

    // duk_push_c_function(ctx, bitsyDrawEnd, 0);
    // duk_put_global_string(ctx, "bitsyDrawEnd");

    // duk_push_c_function(ctx, bitsyDrawPixel, 3);
    // duk_put_global_string(ctx, "bitsyDrawPixel");

    // duk_push_c_function(ctx, bitsyDrawTile, 3);
    // duk_put_global_string(ctx, "bitsyDrawTile");

    // duk_push_c_function(ctx, bitsyDrawTextbox, 2);
    // duk_put_global_string(ctx, "bitsyDrawTextbox");

    // duk_push_c_function(ctx, bitsyClear, 1);
    // duk_put_global_string(ctx, "bitsyClear");

    // duk_push_c_function(ctx, bitsyAddTile, 0);
    // duk_put_global_string(ctx, "bitsyAddTile");

    // duk_push_c_function(ctx, bitsyResetTiles, 0);
    // duk_put_global_string(ctx, "bitsyResetTiles");

    // duk_push_c_function(ctx, bitsySetTextboxSize, 2);
    // duk_put_global_string(ctx, "bitsySetTextboxSize");

    duk_push_c_function(ctx, bitsyOnLoad, 1);
    duk_put_global_string(ctx, "bitsyOnLoad");

    duk_push_c_function(ctx, bitsyOnQuit, 1);
    duk_put_global_string(ctx, "bitsyOnQuit");

    duk_push_c_function(ctx, bitsyOnUpdate, 1);
    duk_put_global_string(ctx, "bitsyOnUpdate");
}

void run_bitsy_duktape(void)
{
    duk_context *ctx = NULL;
    ctx = duk_create_heap(duk_psram_alloc, duk_psram_realloc, duk_psram_free, NULL, duk_fatal_error);

    if (!ctx)
    {
        ESP_LOGE(TAG, "Failed to create Duktape heap");
        return;
    }

    ESP_LOGI(TAG, "Duktape heap created successfully!");

    register_bitsy_api(ctx);
    ESP_LOGI(TAG, "Bitsy API registered");

    // register print function
    duk_push_c_function(ctx, duk_print, DUK_VARARGS);
    duk_put_global_string(ctx, "print");

    log_mem();

    // Load the Bitsy engine scripts
    bool engine_loaded = duk_load_bitsy_engine(ctx);
    if (!engine_loaded)
    {
        ESP_LOGE(TAG, "Failed to load Bitsy engine scripts");
        return;
    }
    ESP_LOGI(TAG, "Bitsy engine loaded");

    log_mem();

    // sleep for a while
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Clean up and destroy the Duktape heap
    duk_destroy_heap(ctx);

    ESP_LOGI(TAG, "Duktape heap destroyed and program completed.");
}

void app_main(void)
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

    // Declare the wear leveling handle
    wl_handle_t s_wl_handle;

    // Mount FAT filesystem with wear leveling
    err = esp_vfs_fat_spiflash_mount_rw_wl("/spiflash", "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    // Run bitsy engine
    run_bitsy_duktape();

    // Unmount FAT filesystem
    err = esp_vfs_fat_spiflash_unmount_rw_wl("/spiflash", s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS (%s)", esp_err_to_name(err));
    }

    // Deinitialize NVS
    nvs_flash_deinit();

    ESP_LOGI(TAG, "Program completed.");
}