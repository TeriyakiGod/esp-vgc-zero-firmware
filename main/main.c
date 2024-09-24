#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "duktape.h"

#define DUKTAPE_HEAP_SIZE (4 * 1024 * 1024)  // 4MB

static const char *TAG = "duktape_example";

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

void run_duktape(void) {
    ESP_LOGI(TAG, "Initializing Duktape heap in PSRAM...");

    // Check available PSRAM
    ESP_LOGI(TAG, "Available heap in PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Create a Duktape heap with custom allocators that use PSRAM
    duk_context *ctx = duk_create_heap(duk_psram_alloc, duk_psram_realloc, duk_psram_free, NULL, NULL);

    if (!ctx) {
        ESP_LOGE(TAG, "Failed to create Duktape heap");
        return;
    }

    ESP_LOGI(TAG, "Duktape heap created successfully!");

    // Register the custom 'print' function
    duk_push_c_function(ctx, duk_print, DUK_VARARGS);
    duk_put_global_string(ctx, "print");

    // Run a simple Duktape script
    const char *script = "print('Hello from Duktape in PSRAM!');";
    if (duk_peval_string(ctx, script) != 0) {
        ESP_LOGE(TAG, "Duktape script error: %s", duk_safe_to_string(ctx, -1));
    }

    // Clean up and destroy the Duktape heap
    duk_destroy_heap(ctx);

    ESP_LOGI(TAG, "Duktape heap destroyed and program completed.");
}


void app_main(void) {
    ESP_LOGI(TAG, "Starting Duktape PSRAM example...");
    run_duktape();

    // Sleep indefinitely to prevent restart
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep for 1 second in an infinite loop
    }
}
