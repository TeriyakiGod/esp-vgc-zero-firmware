#include "bitsybox.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "display.h"

static const char *TAG = "BitsyBox";

lv_obj_t *canvas;
lv_color_t *canvas_buffer;

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

bool duk_load_precompiled_script(duk_context *ctx, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open bytecode file: %s", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory for the bytecode
    char *bytecode = heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
    if (bytecode)
    {
        fread(bytecode, 1, length, f);
        fclose(f);

        // Push bytecode as a buffer, not as a string
        void *buf = duk_push_fixed_buffer(ctx, length);
        memcpy(buf, bytecode, length);

        // Load the precompiled function from the bytecode buffer
        duk_load_function(ctx);

        // Execute the loaded function (e.g., global scope)
        if (duk_pcall(ctx, 0) != 0)
        {
            ESP_LOGE(TAG, "Bytecode execution error: %s\n", duk_safe_to_string(ctx, -1));
            heap_caps_free(bytecode);
            return false;
        }

        // Free the bytecode buffer
        heap_caps_free(bytecode);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for bytecode.");
        fclose(f);
        return false;
    }

    return true;
}

bool duk_load_file(duk_context *ctx, const char *filepath, const char *globalName)
{
    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open font file: %s", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *fontData = heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
    if (fontData)
    {
        fread(fontData, 1, length, f);
        fclose(f);

        // Load the font data onto the Duktape stack
        duk_push_lstring(ctx, fontData, length);
        duk_put_global_string(ctx, globalName);

        // Free font data buffer after loading
        heap_caps_free(fontData);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory for font data.");
        fclose(f);
        return false;
    }

    return true;
}

bool duk_load_bitsy_engine(duk_context *ctx)
{
    bool success = true;

    // load system and engine scripts
    const char *scripts[] = {
        "/spiflash/bitsy/system/system.bin",
        "/spiflash/bitsy/engine/world.bin",
        "/spiflash/bitsy/engine/sound.bin",
        "/spiflash/bitsy/engine/font.bin",
        "/spiflash/bitsy/engine/transition.bin",
        "/spiflash/bitsy/engine/script.bin",
        "/spiflash/bitsy/engine/dialog.bin",
        "/spiflash/bitsy/engine/renderer.bin",
        "/spiflash/bitsy/engine/bitsy.bin"};

    for (int i = 0; i < sizeof(scripts) / sizeof(scripts[0]); i++)
    {
        if (!duk_load_precompiled_script(ctx, scripts[i]))
        {
            ESP_LOGE(TAG, "Failed to load script: %s", scripts[i]);
            success = false;
        }
    }

    // load font
    const char *font_path = "/spiflash/bitsy/font/ascii_small.bitsyfont";
    if (!duk_load_file(ctx, font_path, "__bitsybox_default_font__"))
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
        lvgl_port_lock(0);
        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);
        // copy screen buffer to canvas buffer
        for (int i = 0; i < SCREEN_SIZE * SCREEN_SIZE; i++)
        {
            lv_canvas_set_px(canvas, i % SCREEN_SIZE, i / SCREEN_SIZE, drawingBuffers[SCREEN_BUFFER_ID][i], LV_OPA_COVER);
        }
        lv_canvas_finish_layer(canvas, &layer);
        lvgl_port_unlock();

        // Exit game if all buttons are pressed
        if (isButtonUp && isButtonDown && isButtonLeft && isButtonRight)
        {
            // set game over flag
            duk_peval_string(ctx, "__bitsybox_is_game_over__ = true;");
            duk_pop(ctx);
        }

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
    systemPalette[0] = lv_color_make(255, 0, 0); // red
    systemPalette[1] = lv_color_make(0, 255, 0); // green
    systemPalette[2] = lv_color_make(0, 0, 255); // blue

    canvas_buffer = heap_caps_malloc(128 * 128 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    lvgl_port_lock(0);
    canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(canvas, canvas_buffer, 128, 128, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas);
    lv_canvas_fill_bg(canvas, lv_color_make(0, 0, 255), LV_OPA_COVER);
    lvgl_port_unlock();

    // Initialize drawing buffers
    drawingBuffers[0] = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM); // screen buffer
    if (drawingBuffers[0] == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for screen buffer");
        return;
    }

    drawingBuffers[1] = heap_caps_malloc(104 * 38 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM); // textbox buffer
    if (drawingBuffers[1] == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
        heap_caps_free(drawingBuffers[0]);
        return;
    }

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
    if (!duk_load_file(ctx, GAME_DATA_FILE_PATH, "__bitsybox_game_data__"))
    {
        ESP_LOGE(TAG, "Failed to load game data: %s", GAME_DATA_FILE_PATH);
        return;
    }

    log_mem();

    // initialize input
    init_input();

    // Run the game loop
    duk_run_bitsy_game_loop(ctx);

    log_mem();

    // Free buffers
    heap_caps_free(canvas_buffer);
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