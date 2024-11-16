#include "bitsybox.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "display.h"
#include "perfmon.h"

static const char *TAG = "BitsyBox";

uint16_t systemPalette[SYSTEM_PALETTE_MAX];
uint16_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

static int isGameOver = 0;
static const char *gameFilePath = "/spiflash/bitsy/games/mossland.bitsy";

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
    // load engine scripts
    const char *scripts[] = {
        "/spiflash/bitsy/engine/script.bin",
        "/spiflash/bitsy/engine/font.bin",
        "/spiflash/bitsy/engine/transition.bin",
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

void duk_load_game(duk_context *ctx)
{
    if (duk_peval_string(ctx, "__bitsybox_on_load__(__bitsybox_game_data__, __bitsybox_default_font__);") != 0)
    {
        printf("Load Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}

void duk_init_bitsy_game(duk_context *ctx)
{
    // set game over flag
    duk_peval_string(ctx, "var __bitsybox_is_game_over__ = false;");
    duk_pop(ctx);
    // Load game
    duk_load_game(ctx);
}

void duk_update_game_state(duk_context *ctx)
{
    if (duk_peval_string(ctx, "__bitsybox_on_update__();") != 0)
    {
        printf("Update Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}

void display_draw()
{
    ESP_ERROR_CHECK(vgc_lcd_draw_bitmap(0, 0, SCREEN_SIZE, SCREEN_SIZE, drawingBuffers[SCREEN_BUFFER_ID]));
}

void duk_check_game_over(duk_context *ctx)
{
    // Exit game if all buttons are pressed
    if (isButtonUp && isButtonDown && isButtonLeft && isButtonRight)
    {
        // set game over flag
        duk_peval_string(ctx, "__bitsybox_is_game_over__ = true;");
        duk_pop(ctx);
    }

    if (duk_peval_string(ctx, "__bitsybox_is_game_over__") != 0)
    {
        printf("Test Game Over Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    isGameOver = duk_get_boolean(ctx, -1);
    duk_pop(ctx);
}

void duk_quit_bitsy_game(duk_context *ctx)
{
    if (duk_peval_string(ctx, "__bitsybox_on_quit__();") != 0)
    {
        printf("Quit Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}

void duk_init_bitsy_system(duk_context *ctx)
{
    // Initialize drawing buffers
    drawingBuffers[0] = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA); // screen buffer
    if (drawingBuffers[0] == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for screen buffer");
        return;
    }
    drawingBuffers[1] = heap_caps_malloc(104 * 38 * sizeof(uint16_t), MALLOC_CAP_SPIRAM); // textbox buffer
    if (drawingBuffers[1] == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
        heap_caps_free(drawingBuffers[0]);
        return;
    }
    // Register Bitsy API
    duk_register_bitsy_api(ctx);
    // Load the Bitsy engine scripts
    if (!duk_load_bitsy_engine(ctx))
    {
        ESP_LOGE(TAG, "Failed to load Bitsy engine scripts");
        return;
    }
    ESP_LOGI(TAG, "Bitsy engine loaded");
    // Load game data
    if (!duk_load_file(ctx, gameFilePath, "__bitsybox_game_data__"))
    {
        ESP_LOGE(TAG, "Failed to load game data: %s", gameFilePath);
        return;
    }
    ESP_LOGI(TAG, "Game data loaded");
}

duk_context *init_duktape_heap()
{
    // Create Duktape heap
    duk_context *ctx = NULL;
    ctx = duk_create_heap(duk_psram_alloc, duk_psram_realloc, duk_psram_free, NULL, duk_fatal_error);
    if (!ctx)
    {
        ESP_LOGE(TAG, "Failed to create Duktape heap");
        return NULL;
    }
    ESP_LOGI(TAG, "Duktape heap created successfully!");
    return ctx;
}

void deinit_bitsy_system()
{
    for (int i = 0; i < SYSTEM_DRAWING_BUFFER_MAX; i++)
    {
        if (drawingBuffers[i])
        {
            heap_caps_free(drawingBuffers[i]);
        }
    }
}

void log_performance(const char *function_name, int64_t start_time, int64_t end_time)
{
    float time_ms = (end_time - start_time) / 1000.0; // Convert microseconds to milliseconds
    printf("%s took %.2f ms\n", function_name, time_ms);
}

void vBitsyEngineTask(void *pvParameters)
{
    // Initialize Duktape heap
    duk_context *ctx = init_duktape_heap();
    // Initialize Bitsy system
    duk_init_bitsy_system(ctx);
    // Initialize Bitsy game
    duk_init_bitsy_game(ctx);
    // Initialize input GPIO
    init_input_gpio();

    for (;;)
    {
        duk_update_game_state(ctx);
        display_draw();
        duk_check_game_over(ctx);
        vTaskDelay(1);
    }

    // Perform cleanup
    duk_quit_bitsy_game(ctx);
    deinit_bitsy_system();
    duk_destroy_heap(ctx);

    vTaskDelete(NULL);
}
