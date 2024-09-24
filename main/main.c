#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "duktape.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "nvs_flash.h"


#define DUKTAPE_HEAP_SIZE (4 * 1024 * 1024)  // 4MB

static const char *TAG = "vgc_firmware";

// Allocate heap in PSRAM
void *duk_psram_alloc(void *udata, duk_size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void *duk_psram_realloc(void *udata, void *ptr, duk_size_t size) {
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
}

void duk_psram_free(void *udata, void *ptr) {
    heap_caps_free(ptr);
}

static void fatalError(void *udata, const char *msg)
{
    ESP_LOGE(TAG, "Fatal error: %s", msg);
}

void mount_flash_storage(){
    ESP_LOGI(TAG, "Mounting FAT filesystem from flash...");

    esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount("/spiflash", "storage", &mount_config, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FAT filesystem. Error: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "FAT filesystem mounted successfully!");
}

void unmount_flash_storage(){
    ESP_LOGI(TAG, "Unmounting FAT filesystem from flash...");

    esp_err_t err = esp_vfs_fat_spiflash_unmount("/spiflash", NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount FAT filesystem. Error: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "FAT filesystem unmounted successfully!");
}

int loadScript(duk_context* ctx, const char* filepath) {
    int success = 0;

    printf("Loading %s ...\n", filepath);

    char* fileBuffer = NULL;
    long length = 0;
    FILE* f = fopen(filepath, "r");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);

        fileBuffer = malloc(length + 1);
        if (fileBuffer) {
            length = fread(fileBuffer, 1, length, f);
            fileBuffer[length] = '\0';  // Null terminate
        } else {
            printf("Error: Failed to allocate memory for file %s\n", filepath);
            fclose(f);
            return 0;
        }

        fclose(f);
    } else {
        printf("Error: Unable to open file %s\n", filepath);
        return 0;
    }

    if (fileBuffer) {
        duk_push_lstring(ctx, (const char *)fileBuffer, (duk_size_t)length);

        if (duk_peval(ctx) != 0) {
            printf("Script Evaluation Error: %s\n", duk_safe_to_string(ctx, -1));
        } else {
            success = 1;  // Successfully executed script
        }
        duk_pop(ctx);
        free(fileBuffer);  // Free buffer after use
    }

    return success;
}

// Updated `loadFile` function
int loadFile(duk_context* ctx, const char* filepath, const char* variableName) {
    int success = 0;

    printf("Loading %s ...\n", filepath);

    char* fileBuffer = NULL;
    long length = 0;
    FILE* f = fopen(filepath, "r");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);

        fileBuffer = malloc(length + 1);
        if (fileBuffer) {
            length = fread(fileBuffer, 1, length, f);
            fileBuffer[length] = '\0';  // Null terminate
        } else {
            printf("Error: Failed to allocate memory for file %s\n", filepath);
            fclose(f);
            return 0;
        }

        fclose(f);
    } else {
        printf("Error: Unable to open file %s\n", filepath);
        return 0;
    }

    if (fileBuffer) {
        duk_push_lstring(ctx, (const char *)fileBuffer, (duk_size_t)length);
        duk_put_global_string(ctx, variableName);  // Set global variable
        success = 1;
        free(fileBuffer);  // Free buffer after use
    }

    return success;
}

// Function to print memory usage
void print_memory_info() {
    size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t free_psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    printf("Memory Info after script load:\n");
    printf("  Free heap size: %d bytes\n", free_heap_size);
    printf("  Free PSRAM size: %d bytes\n", free_psram_size);
}

// Custom Duktape 'print' function to redirect to ESP-IDF logging
static duk_ret_t duk_print(duk_context *ctx) {
    int nargs = duk_get_top(ctx);  // Get number of arguments
    for (int i = 0; i < nargs; i++) {
        if (i > 0) {
            ESP_LOGI("Duktape", " ");  // Print a space between arguments
        }
        ESP_LOGI("Duktape", "%s", duk_safe_to_string(ctx, i));  // Print each argument
    }
    return 0;  // No return value
}

void loadBitsyEngine(duk_context* ctx) {
    // load engine
    loadScript(ctx, "bitsy/engine/script.js");
    loadScript(ctx, "bitsy/engine/font.js");
    loadScript(ctx, "bitsy/engine/transition.js");
    loadScript(ctx, "bitsy/engine/dialog.js");
    loadScript(ctx, "bitsy/engine/renderer.js");
    loadScript(ctx, "bitsy/engine/bitsy.js");
    // load default font
    loadFile(ctx, "bitsy/font/ascii_small.bitsyfont", "__bitsybox_default_font__");
}

duk_ret_t bitsyLog(duk_context *ctx)
{
    ESP_LOGI("Bitsy", "%s", duk_safe_to_string(ctx, 0));
    return 0;
}

duk_ret_t bitsyGetButton(duk_context *ctx) {
    // TODO: implement
    duk_push_int(ctx, 0);  // Return 0 by default (no button pressed)
    return 1;  // Return one result
}

duk_ret_t bitsySetGraphicsMode(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsySetColor(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyResetColors(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyDrawBegin(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyDrawEnd(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyDrawPixel(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyDrawTile(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyDrawTextbox(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyClear(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsyAddTile(duk_context *ctx)
{
    // TODO: implement
    return 1;
}

duk_ret_t bitsyResetTiles(duk_context *ctx)
{
    // TODO: implement
    return 0;
}

duk_ret_t bitsySetTextboxSize(duk_context *ctx)
{
    // TODO: implement
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

void registerBitsyApi(duk_context* ctx) {
	duk_push_c_function(ctx, bitsyLog, 2);
	duk_put_global_string(ctx, "bitsyLog");

	duk_push_c_function(ctx, bitsyGetButton, 1);
	duk_put_global_string(ctx, "bitsyGetButton");

	duk_push_c_function(ctx, bitsySetGraphicsMode, 1);
	duk_put_global_string(ctx, "bitsySetGraphicsMode");

	duk_push_c_function(ctx, bitsySetColor, 4);
	duk_put_global_string(ctx, "bitsySetColor");

	duk_push_c_function(ctx, bitsyResetColors, 0);
	duk_put_global_string(ctx, "bitsyResetColors");

	duk_push_c_function(ctx, bitsyDrawBegin, 1);
	duk_put_global_string(ctx, "bitsyDrawBegin");

	duk_push_c_function(ctx, bitsyDrawEnd, 0);
	duk_put_global_string(ctx, "bitsyDrawEnd");

	duk_push_c_function(ctx, bitsyDrawPixel, 3);
	duk_put_global_string(ctx, "bitsyDrawPixel");

	duk_push_c_function(ctx, bitsyDrawTile, 3);
	duk_put_global_string(ctx, "bitsyDrawTile");

	duk_push_c_function(ctx, bitsyDrawTextbox, 2);
	duk_put_global_string(ctx, "bitsyDrawTextbox");

	duk_push_c_function(ctx, bitsyClear, 1);
	duk_put_global_string(ctx, "bitsyClear");

	duk_push_c_function(ctx, bitsyAddTile, 0);
	duk_put_global_string(ctx, "bitsyAddTile");

	duk_push_c_function(ctx, bitsyResetTiles, 0);
	duk_put_global_string(ctx, "bitsyResetTiles");

	duk_push_c_function(ctx, bitsySetTextboxSize, 2);
	duk_put_global_string(ctx, "bitsySetTextboxSize");

	duk_push_c_function(ctx, bitsyOnLoad, 1);
	duk_put_global_string(ctx, "bitsyOnLoad");

	duk_push_c_function(ctx, bitsyOnQuit, 1);
	duk_put_global_string(ctx, "bitsyOnQuit");

	duk_push_c_function(ctx, bitsyOnUpdate, 1);
	duk_put_global_string(ctx, "bitsyOnUpdate");
}

void run_bitsy_duktape(void) {
    ESP_LOGI(TAG, "Initializing Duktape heap in PSRAM...");

    // Create a Duktape heap with custom allocators that use PSRAM
    duk_context *ctx = duk_create_heap(duk_psram_alloc, duk_psram_realloc, duk_psram_free, NULL, fatalError);

    if (!ctx) {
        ESP_LOGE(TAG, "Failed to create Duktape heap");
        return;
    }

    ESP_LOGI(TAG, "Duktape heap created successfully!");

    registerBitsyApi(ctx);
    ESP_LOGI(TAG, "Bitsy API registered");

    // Load the Bitsy engine scripts
    loadBitsyEngine(ctx);
    ESP_LOGI(TAG, "Bitsy engine loaded");

    // Print memory usage after loading the scripts
    print_memory_info();

    // Clean up and destroy the Duktape heap
    duk_destroy_heap(ctx);

    ESP_LOGI(TAG, "Duktape heap destroyed and program completed.");
}

void app_main(void) {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS. Error: %s", esp_err_to_name(err));
        return;
    }

    // Mount FAT filesystem from flash
    mount_flash_storage();

    // Run bitsy engine
    run_bitsy_duktape();

    // sleep for a while
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Unmount FAT filesystem from flash
    unmount_flash_storage();

    // Deinitialize NVS
    nvs_flash_deinit();

    ESP_LOGI(TAG, "Program completed.");
}
