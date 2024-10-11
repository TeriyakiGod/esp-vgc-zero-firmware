#include "bitsybox.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "display.h"

static const char *TAG = "BitsyBox";

uint16_t systemPalette[SYSTEM_PALETTE_MAX];
uint16_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

typedef enum
{
    FILETYPE_SCRIPT,
    FILETYPE_FILE
} filetype_t;

static void log_mem()
{
    ESP_LOGI(TAG, "PSRAM left %d KB", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI(TAG, "RAM left %" PRIu32 " KB", (esp_get_free_heap_size() - heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / 1024);
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

bool duk_load(duk_context *ctx, const char *filepath, filetype_t type, const char *variableName)
{
    bool success = true;

    ESP_LOGI(TAG, "Loading: %s", filepath);

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
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for file: %s", filepath);
            success = false;
        }

        fclose(f);
    }
    else
    {
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

    // load engine scripts
    const char *scripts[] = {
        "/spiflash/bitsy/engine/script.js",
        "/spiflash/bitsy/engine/font.js",
        "/spiflash/bitsy/engine/transition.js",
        "/spiflash/bitsy/engine/dialog.js",
        "/spiflash/bitsy/engine/renderer.js",
        "/spiflash/bitsy/engine/bitsy.js"};

    for (int i = 0; i < sizeof(scripts) / sizeof(scripts[0]); i++)
    {
        if (!duk_load(ctx, scripts[i], FILETYPE_SCRIPT, NULL))
        {
            ESP_LOGE(TAG, "Failed to load script: %s", scripts[i]);
            success = false;
        }
    }

    // load font
    const char *font_path = "/spiflash/bitsy/font/ascii_small.bitsyfont";
    if (!duk_load(ctx, font_path, FILETYPE_FILE, "__bitsybox_default_font__"))
    {
        ESP_LOGE(TAG, "Failed to load font: %s", font_path);
        success = false;
    }
    return success;
}

void duk_run_bitsy_game_loop(duk_context *ctx)
{
    // set game over flag
    int isGameOver = 0;
    duk_peval_string(ctx, "var __bitsybox_is_game_over__ = false;");
    duk_pop(ctx);

    // Load game
    if (duk_peval_string(ctx, "__bitsybox_on_load__(__bitsybox_game_data__, __bitsybox_default_font__);") != 0)
    {
        printf("Load Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);

    // Main game loop
    while (!isGameOver)
    {
        // Get input
        get_input();

        // Update game state
        if (duk_peval_string(ctx, "__bitsybox_on_update__();") != 0)
        {
            printf("Update Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
        }
        duk_pop(ctx);

        // Draw screen buffer to LCD
        ESP_ERROR_CHECK(vgc_lcd_draw_bitmap(0, 0, SCREEN_SIZE, SCREEN_SIZE, drawingBuffers[SCREEN_BUFFER_ID]));

        // Check if game over
        if (duk_peval_string(ctx, "__bitsybox_is_game_over__") != 0)
        {
            printf("Test Game Over Error: %s\n", duk_safe_to_string(ctx, -1));
        }
        isGameOver = duk_get_boolean(ctx, -1);
        duk_pop(ctx);
    }

    // Quit game
    if (duk_peval_string(ctx, "__bitsybox_on_quit__();") != 0)
    {
        printf("Quit Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}

void app_duktape_bitsy()
{
    // Initialize system palette
    systemPalette[0] = (31 << 11) | (0 << 5) | 0;         // Red
    systemPalette[1] = (0 << 11) | (31 << 5) | 0;         // Green
    systemPalette[2] = (0 << 11) | (0 << 5) | 31;         // Blue

    // Initialize drawing buffers
    drawingBuffers[0] = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);  // screen buffer
    if (drawingBuffers[0] == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for screen buffer");
        return;
    }

    log_mem();

    drawingBuffers[1] = heap_caps_malloc(104 * 38 * sizeof(uint16_t), MALLOC_CAP_DMA);  // textbox buffer
    if (drawingBuffers[1] == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
        heap_caps_free(drawingBuffers[0]);
        return;
    }

    log_mem();

    // Create Duktape heap
    duk_context *ctx = NULL;
    ctx = duk_create_heap(duk_psram_alloc, duk_psram_realloc, duk_psram_free, NULL, duk_fatal_error);

    if (!ctx)
    {
        ESP_LOGE(TAG, "Failed to create Duktape heap");
        return;
    }

    ESP_LOGI(TAG, "Duktape heap created successfully!");

    // Register Bitsy API
    register_bitsy_api(ctx);
    ESP_LOGI(TAG, "Bitsy API registered");

    log_mem();

    // Load the Bitsy engine scripts
    bool engine_loaded = duk_load_bitsy_engine(ctx);
    if (!engine_loaded)
    {
        ESP_LOGE(TAG, "Failed to load Bitsy engine scripts");
        return;
    }
    ESP_LOGI(TAG, "Bitsy engine loaded");

    // Load game data
    const char *gameFilePath = "/spiflash/bitsy/games/mossland.bitsy";
    if (!duk_load(ctx, gameFilePath, FILETYPE_FILE, "__bitsybox_game_data__"))
    {
        ESP_LOGE(TAG, "Failed to load game data: %s", gameFilePath);
        return;
    }

    log_mem();

    // initialize input
    init_input();

    // Run the game loop
    duk_run_bitsy_game_loop(ctx);

    log_mem();

    // Free buffers
    for (int i = 0; i < SYSTEM_DRAWING_BUFFER_MAX; i++)
    {
        if (drawingBuffers[i])
        {
            heap_caps_free(drawingBuffers[i]);
        }
    }

    // Clean up and destroy the Duktape heap
    duk_destroy_heap(ctx);

    ESP_LOGI(TAG, "Duktape heap destroyed and program completed.");
}