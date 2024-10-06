#include "bitsybox.h"
#include <stdio.h>
#include "duktape.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "display.h"

#define SYSTEM_PALETTE_MAX 256
#define SYSTEM_DRAWING_BUFFER_MAX 1024

static const char *TAG = "BitsyBox";

/* GAME SELECT */
char gameFilePath[256];
int gameCount = 0;

/* GLOBALS */
int screenSize = 128;
int tileSize = 8;
int roomSize = 16;

int shouldContinue = 1;

int curGraphicsMode = 0;

static uint16_t systemPalette[SYSTEM_PALETTE_MAX];
static uint16_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

int curBufferId = -1;
int screenBufferId = 0;
int textboxBufferId = 1;
int tileStartBufferId = 2;
int nextBufferId = 2;

int textboxWidth = 100;
int textboxHeight = 64;

/* INPUT */
int isButtonUp = 0;
int isButtonDown = 0;
int isButtonLeft = 0;
int isButtonRight = 0;
int isButtonW = 0;
int isButtonA = 0;
int isButtonS = 0;
int isButtonD = 0;
int isButtonR = 0;
int isButtonSpace = 0;
int isButtonReturn = 0;
int isButtonEscape = 0;
int isButtonLCtrl = 0;
int isButtonRCtrl = 0;
int isButtonLAlt = 0;
int isButtonRAlt = 0;
int isButtonPadUp = 0;
int isButtonPadDown = 0;
int isButtonPadLeft = 0;
int isButtonPadRight = 0;
int isButtonPadA = 0;
int isButtonPadB = 0;
int isButtonPadX = 0;
int isButtonPadY = 0;
int isButtonPadStart = 0;

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
        // log_mem();
        //  Load scripts
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

duk_ret_t bitsy_get_button(duk_context *ctx)
{
    int buttonCode = duk_get_int(ctx, 0);

    int isAnyAlt = (isButtonLAlt || isButtonRAlt);
    int isAnyCtrl = (isButtonLCtrl || isButtonRCtrl);
    int isCtrlPlusR = isAnyCtrl && isButtonR;
    int isPadFaceButton = isButtonPadA || isButtonPadB || isButtonPadX || isButtonPadY;

    if (buttonCode == 0)
    {
        duk_push_boolean(ctx, isButtonUp || isButtonW || isButtonPadUp);
    }
    else if (buttonCode == 1)
    {
        duk_push_boolean(ctx, isButtonDown || isButtonS || isButtonPadDown);
    }
    else if (buttonCode == 2)
    {
        duk_push_boolean(ctx, isButtonLeft || isButtonA || isButtonPadLeft);
    }
    else if (buttonCode == 3)
    {
        duk_push_boolean(ctx, isButtonRight || isButtonD || isButtonPadRight);
    }
    else if (buttonCode == 4)
    {
        duk_push_boolean(ctx, isButtonSpace || (isButtonReturn && !isAnyAlt) || isPadFaceButton);
    }
    else if (buttonCode == 5)
    {
        duk_push_boolean(ctx, isButtonEscape || isCtrlPlusR || isButtonPadStart);
    }
    else
    {
        duk_push_boolean(ctx, 0);
    }

    return 1;
}

duk_ret_t bitsy_log(duk_context *ctx)
{
    const char *printStr;
    printStr = duk_safe_to_string(ctx, 0);

    ESP_LOGI(TAG, "Bitsy: %s", printStr);
    return 0;
}

duk_ret_t bitsy_set_graphics_mode(duk_context *ctx)
{
    curGraphicsMode = duk_get_int(ctx, 0);

    return 0;
}

duk_ret_t bitsy_set_color(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    int r = duk_get_int(ctx, 1);
    int g = duk_get_int(ctx, 2);
    int b = duk_get_int(ctx, 3);

    systemPalette[paletteIndex] = (r << 11) | (g << 5) | b;

    return 0;
}

duk_ret_t bitsy_reset_colors(duk_context *ctx)
{
    for (int i = 0; i < SYSTEM_PALETTE_MAX; i++)
    {
        systemPalette[i] = 0;
    }

    return 0;
}

duk_ret_t bitsy_draw_begin(duk_context *ctx)
{
    curBufferId = duk_get_int(ctx, 0);
    return 0;
}

duk_ret_t bitsy_draw_end(duk_context *ctx)
{
    curBufferId = -1;
    return 0;
}

duk_ret_t bitsyDrawPixel(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    drawingBuffers[curBufferId][y * screenSize + x] = systemPalette[paletteIndex];

    return 0;
}

duk_ret_t bitsyDrawTile(duk_context *ctx)
{
    // can only draw tiles on the screen buffer in tile mode
    if (curBufferId != 0 || curGraphicsMode != 1)
    {
        return 0;
    }

    int tileId = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }

    // copy tile to screen buffer
    for (int ty = 0; ty < tileSize; ty++)
    {
        for (int tx = 0; tx < tileSize; tx++)
        {
            uint16_t color = drawingBuffers[tileId][ty * tileSize + tx];
            drawingBuffers[screenBufferId][(y + ty) * screenSize + (x + tx)] = color;
        }
    }

    return 0;
}

duk_ret_t bitsyDrawTextbox(duk_context *ctx)
{
    if (curBufferId != 0 || curGraphicsMode != 1)
    {
        return 0;
    }

    int x = duk_get_int(ctx, 0);
    int y = duk_get_int(ctx, 1);

    // Copy textbox buffer to screen buffer
    for (int ty = 0; ty < textboxHeight; ty++)
    {
        for (int tx = 0; tx < textboxWidth; tx++)
        {
            uint16_t color = drawingBuffers[textboxBufferId][ty * textboxWidth + tx];
            drawingBuffers[screenBufferId][(y + ty) * screenSize + (x + tx)] = color;
        }
    }

    return 0;
}

duk_ret_t bitsyClear(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);

    uint16_t color = systemPalette[paletteIndex];

    if (curBufferId == 0)
    {
        // Clear the screen buffer
        for (int i = 0; i < screenSize * screenSize; i++)
        {
            drawingBuffers[screenBufferId][i] = color;
        }
    }
    else if (curBufferId == 1)
    {
        // Clear the textbox buffer
        for (int i = 0; i < textboxWidth * textboxHeight; i++)
        {
            drawingBuffers[textboxBufferId][i] = color;
        }
    }
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId)
    {
        // Clear the tile buffer
        for (int i = 0; i < tileSize * tileSize; i++)
        {
            drawingBuffers[curBufferId][i] = color;
        }
    }

    return 0;
}

duk_ret_t bitsyAddTile(duk_context *ctx)
{
    if (nextBufferId >= SYSTEM_DRAWING_BUFFER_MAX)
    {
        // todo : error handling?
        return 0;
    }

    // allocate a new tile buffer
    drawingBuffers[nextBufferId] = heap_caps_malloc(tileSize * tileSize * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!drawingBuffers[nextBufferId])
    {
        ESP_LOGE(TAG, "Failed to allocate memory for tile buffer");
    }

    duk_push_int(ctx, nextBufferId);

    nextBufferId++;

    return 1;
}

duk_ret_t bitsyResetTiles(duk_context *ctx)
{
    nextBufferId = tileStartBufferId;

    return 0;
}

duk_ret_t bitsySetTextboxSize(duk_context *ctx)
{
    textboxWidth = duk_get_int(ctx, 0);
    textboxHeight = duk_get_int(ctx, 1);

    heap_caps_free(drawingBuffers[textboxBufferId]);
    drawingBuffers[textboxBufferId] = heap_caps_malloc(textboxWidth * textboxHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!drawingBuffers[textboxBufferId])
    {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
    }

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

static void register_bitsy_api(duk_context *ctx)
{
    duk_push_c_function(ctx, bitsy_log, 2);
    duk_put_global_string(ctx, "bitsyLog");

    duk_push_c_function(ctx, bitsy_get_button, 1);
    duk_put_global_string(ctx, "bitsyGetButton");

    duk_push_c_function(ctx, bitsy_set_graphics_mode, 1);
    duk_put_global_string(ctx, "bitsySetGraphicsMode");

    duk_push_c_function(ctx, bitsy_set_color, 4);
    duk_put_global_string(ctx, "bitsySetColor");

    duk_push_c_function(ctx, bitsy_reset_colors, 0);
    duk_put_global_string(ctx, "bitsyResetColors");

    duk_push_c_function(ctx, bitsy_draw_begin, 1);
    duk_put_global_string(ctx, "bitsyDrawBegin");

    duk_push_c_function(ctx, bitsy_draw_end, 0);
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

void gameLoop(duk_context *ctx)
{
    // loop time

    int64_t prevTime = esp_timer_get_time();
    int64_t currentTime = 0;
    int64_t deltaTime = 0;
    int64_t loopTime = 0;
    const int64_t loopTimeMax = 16000; // 16 ms for ~60fps

    int isGameOver = 0;

    duk_peval_string(ctx, "var __bitsybox_is_game_over__ = false;");
    duk_pop(ctx);

    // main loop
    if (duk_peval_string(ctx, "__bitsybox_on_load__(__bitsybox_game_data__, __bitsybox_default_font__);") != 0)
    {
        printf("Load Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);

    if (gameCount > 1)
    {
        // hack to return to main menu on game end if there's more than one
        duk_peval_string(ctx, "reset_cur_game = function() { __bitsybox_is_game_over__ = true; };");
        duk_pop(ctx);
    }

    while (shouldContinue && !isGameOver)
    {
        // Get current time
        currentTime = esp_timer_get_time();
        // Calculate delta time in microseconds
        deltaTime = currentTime - prevTime;
        prevTime = currentTime;
        loopTime += deltaTime;

        // printf("dt %d\n", deltaTime); // debug frame rate

        // Get INPUT

        if (loopTime >= loopTimeMax && shouldContinue)
        {
            //lv_color_t bg = systemPalette[0];

            // main loop
            if (duk_peval_string(ctx, "__bitsybox_on_update__();") != 0)
            {
                printf("Update Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx);

            // Draw screen
            ESP_ERROR_CHECK(vgc_lcd_draw_bitmap(0, 0, screenSize, screenSize, drawingBuffers[screenBufferId]));

            loopTime = 0;
        }

        // kind of hacky way to trigger restart
        // if (duk_peval_string(ctx, "if (bitsyGetButton(5)) { reset_cur_game(); }") != 0) {
        // 	printf("Test Restart Game Error: %s\n", duk_safe_to_string(ctx, -1));
        // }
        // duk_pop(ctx);

        if (duk_peval_string(ctx, "__bitsybox_is_game_over__") != 0)
        {
            printf("Test Game Over Error: %s\n", duk_safe_to_string(ctx, -1));
        }
        isGameOver = duk_get_boolean(ctx, -1);
        duk_pop(ctx);
    }

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

    drawingBuffers[0] = heap_caps_malloc(128 * 128 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);  // screen buffer
    if (drawingBuffers[0] == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for screen buffer");
        return;
    }

    log_mem();

    drawingBuffers[1] = heap_caps_malloc(textboxHeight * textboxWidth * sizeof(uint16_t), MALLOC_CAP_SPIRAM);  // textbox buffer
    if (drawingBuffers[1] == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
        heap_caps_free(drawingBuffers[0]);
        return;
    }

    log_mem();

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
    // Load game data
    const char *gameFilePath = "/spiflash/bitsy/games/mossland.bitsy";
    if (!duk_load(ctx, gameFilePath, FILETYPE_FILE, "__bitsybox_game_data__"))
    {
        ESP_LOGE(TAG, "Failed to load game data: %s", gameFilePath);
        return;
    }

    log_mem();

    // Run the game loop
    gameLoop(ctx);

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