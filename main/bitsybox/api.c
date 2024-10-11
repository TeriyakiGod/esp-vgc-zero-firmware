#include "bitsybox.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>

static const char *TAG = "BitsyAPI";

/* GLOBALS */
int curGraphicsMode = 0;

int curBufferId = -1;
int tileStartBufferId = 2;
int nextBufferId = 2;

int textboxWidth = 104;
int textboxHeight = 38;

duk_ret_t bitsy_log(duk_context *ctx)
{
    const char *printStr;
    printStr = duk_safe_to_string(ctx, 0);

    ESP_LOGI(TAG, "Bitsy: %s", printStr);
    return 0;
}

duk_ret_t bitsy_get_button(duk_context *ctx)
{
    int buttonCode = duk_get_int(ctx, 0);

    bool isAnyAlt = (isButtonLAlt || isButtonRAlt);
    bool isAnyCtrl = (isButtonLCtrl || isButtonRCtrl);
    bool isCtrlPlusR = isAnyCtrl && isButtonR;
    bool isPadFaceButton = isButtonPadA || isButtonPadB || isButtonPadX || isButtonPadY;

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

    uint16_t color = (r << 11) | (g << 5) | b;
    ESP_LOGI(TAG, "Setting color (hex: %s) (binary: %s)", itoa(color, (char[17]){0}, 16), itoa(color, (char[17]){0}, 2));
    systemPalette[paletteIndex] = color;

    return 0;
}

duk_ret_t bitsy_reset_colors(duk_context *ctx)
{
    for (int i = 0; i < SYSTEM_PALETTE_MAX; i++)
    {
        systemPalette[i] = 0;
    }
    ESP_LOGI(TAG, "Reset colors");

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

duk_ret_t bitsy_draw_pixel(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    uint16_t color = systemPalette[paletteIndex];

    // Apply render scale
    int scaledX = x * RENDER_SCALE;
    int scaledY = y * RENDER_SCALE;

    if (curBufferId == 0 && curGraphicsMode == 0) {
        // Use scaled coordinates
        for (int i = 0; i < RENDER_SCALE; i++) {
            for (int j = 0; j < RENDER_SCALE; j++) {
                drawingBuffers[SCREEN_BUFFER_ID][(scaledY + j) * SCREEN_SIZE + (scaledX + i)] = color;
            }
        }
    }
    else if (curBufferId == 1 && curGraphicsMode == 1) {
        // Use textboxRenderScale for this buffer
        int scaledTextboxX = x * TEXTBOX_RENDER_SCALE;
        int scaledTextboxY = y * TEXTBOX_RENDER_SCALE;
        for (int i = 0; i < TEXTBOX_RENDER_SCALE; i++) {
            for (int j = 0; j < TEXTBOX_RENDER_SCALE; j++) {
                drawingBuffers[TEXTBOX_BUFFER_ID][(scaledTextboxY + j) * textboxWidth + (scaledTextboxX + i)] = color;
            }
        }
    }
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId && curGraphicsMode == 1) {
        // Use scaled coordinates for tile buffer
        for (int i = 0; i < RENDER_SCALE; i++) {
            for (int j = 0; j < RENDER_SCALE; j++) {
                drawingBuffers[curBufferId][(scaledY + j) * TILE_SIZE + (scaledX + i)] = color;
            }
        }
    }

    return 0;
}

duk_ret_t bitsy_draw_tile(duk_context *ctx)
{
    // Can only draw tiles on the screen buffer in tile mode
    if (curBufferId != 0 || curGraphicsMode != 1) {
        return 0;
    }

    int tileId = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    // Ensure the tileId is valid
    if (tileId < tileStartBufferId || tileId >= nextBufferId) {
        return 0;
    }

    // Calculate the tile position and size with render scale
    int scaledX = x * TILE_SIZE * RENDER_SCALE;
    int scaledY = y * TILE_SIZE * RENDER_SCALE;
    //int scaledTileSize = TILE_SIZE * RENDER_SCALE;

    // Iterate over each pixel of the tile and draw it to the screen buffer
    for (int ty = 0; ty < TILE_SIZE; ty++) {
        for (int tx = 0; tx < TILE_SIZE; tx++) {
            uint16_t color = drawingBuffers[tileId][ty * TILE_SIZE + tx]; // Get the pixel color from the tile buffer

            // Scale the pixel drawing using RENDER_SCALE
            for (int i = 0; i < RENDER_SCALE; i++) {
                for (int j = 0; j < RENDER_SCALE; j++) {
                    int bufferX = scaledX + (tx * RENDER_SCALE) + i;
                    int bufferY = scaledY + (ty * RENDER_SCALE) + j;

                    // Draw the scaled pixel on the screen buffer
                    drawingBuffers[SCREEN_BUFFER_ID][bufferY * SCREEN_SIZE + bufferX] = color;
                }
            }
        }
    }

    return 0;
}

duk_ret_t bitsy_draw_textbox(duk_context* ctx)
{
    // Can only draw the textbox on the screen buffer in tile mode
    if (curBufferId != 0 || curGraphicsMode != 1) {
        return 0;
    }

    int x = duk_get_int(ctx, 0);
    int y = duk_get_int(ctx, 1);

    // Calculate the scaled position of the textbox
    int scaledX = x * TEXTBOX_RENDER_SCALE;
    int scaledY = y * TEXTBOX_RENDER_SCALE;

    // Iterate over each pixel of the textbox buffer and scale it
    for (int ty = 0; ty < textboxHeight; ty++) {
        for (int tx = 0; tx < textboxWidth; tx++) {
            uint16_t color = drawingBuffers[TEXTBOX_BUFFER_ID][ty * textboxWidth + tx]; // Get the pixel color from the textbox buffer

            // Scale the pixel drawing using TEXTBOX_RENDER_SCALE
            for (int i = 0; i < TEXTBOX_RENDER_SCALE; i++) {
                for (int j = 0; j < TEXTBOX_RENDER_SCALE; j++) {
                    int bufferX = scaledX + (tx * TEXTBOX_RENDER_SCALE) + i;
                    int bufferY = scaledY + (ty * TEXTBOX_RENDER_SCALE) + j;

                    // Draw the scaled pixel on the screen buffer
                    drawingBuffers[SCREEN_BUFFER_ID][bufferY * SCREEN_SIZE + bufferX] = color;
                }
            }
        }
    }

    return 0;
}

duk_ret_t bitsy_clear(duk_context* ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);

    uint16_t color = systemPalette[paletteIndex];

    // Clear the screen buffer
    if (curBufferId == 0) {
        for (int y = 0; y < SCREEN_SIZE * RENDER_SCALE; y++) {
            for (int x = 0; x < SCREEN_SIZE * RENDER_SCALE; x++) {
                drawingBuffers[SCREEN_BUFFER_ID][y * SCREEN_SIZE + x] = color;
            }
        }
    }
    // Clear the textbox buffer
    else if (curBufferId == 1) {
        for (int y = 0; y < textboxHeight * TEXTBOX_RENDER_SCALE; y++) {
            for (int x = 0; x < textboxWidth * TEXTBOX_RENDER_SCALE; x++) {
                drawingBuffers[TEXTBOX_BUFFER_ID][y * textboxWidth + x] = color;
            }
        }
    }
    // Clear the tile buffer
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId) {
        for (int y = 0; y < TILE_SIZE * RENDER_SCALE; y++) {
            for (int x = 0; x < TILE_SIZE * RENDER_SCALE; x++) {
                drawingBuffers[curBufferId][y * TILE_SIZE + x] = color;
            }
        }
    }

    return 0;
}

duk_ret_t bitsy_add_tile(duk_context *ctx)
{
    if (nextBufferId >= SYSTEM_DRAWING_BUFFER_MAX)
    {
        // todo : error handling?
        return 0;
    }

    // allocate a new tile buffer
    drawingBuffers[nextBufferId] = heap_caps_malloc(TILE_SIZE * TILE_SIZE * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!drawingBuffers[nextBufferId])
    {
        ESP_LOGE(TAG, "Failed to allocate memory for tile buffer");
    }
    ESP_LOGI(TAG, "Allocated memory for tile buffer %d", nextBufferId);

    duk_push_int(ctx, nextBufferId);

    nextBufferId++;

    return 1;
}

duk_ret_t bitsy_reset_tiles(duk_context *ctx)
{
    nextBufferId = tileStartBufferId;
    ESP_LOGI(TAG, "Reset tiles");

    return 0;
}

duk_ret_t bitsy_set_textbox_size(duk_context* ctx)
{
    // Get the new textbox width and height from the context
    int newTextboxWidth = duk_get_int(ctx, 0);
    int newTextboxHeight = duk_get_int(ctx, 1);

    if (newTextboxWidth == textboxWidth && newTextboxHeight == textboxHeight) {
        // No change in textbox size
        return 0;
    }

    // Free the old buffer if it exists to avoid memory leaks
    if (drawingBuffers[TEXTBOX_BUFFER_ID] != NULL) {
        free(drawingBuffers[TEXTBOX_BUFFER_ID]);
    }

    // Allocate new buffer based on the new textbox size and scale
    int bufferSize = textboxWidth * TEXTBOX_RENDER_SCALE * textboxHeight * TEXTBOX_RENDER_SCALE * sizeof(uint16_t);
    drawingBuffers[TEXTBOX_BUFFER_ID] = (uint16_t*) malloc(bufferSize);

    if (drawingBuffers[TEXTBOX_BUFFER_ID] == NULL) {
        // Handle allocation failure
        return DUK_RET_ERROR;  // Return an error if memory allocation fails
    }

    // Clear the new buffer (initialize with some color, or leave as zero)
    memset(drawingBuffers[TEXTBOX_BUFFER_ID], 0, bufferSize);

    ESP_LOGI(TAG, "Set textbox size to %d x %d", textboxWidth, textboxHeight);

    return 0;  // Return success
}

duk_ret_t bitsy_on_load(duk_context *ctx)
{
    // hacky to just stick it in the global namespace??
    duk_put_global_string(ctx, "__bitsybox_on_load__");
    ESP_LOGI(TAG, "LOADING");

    return 0;
}

duk_ret_t bitsy_on_quit(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_quit__");
    ESP_LOGI(TAG, "QUITTING");
    return 0;
}

duk_ret_t bitsy_on_update(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_update__");

    return 0;
}

void register_bitsy_api(duk_context *ctx)
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

    duk_push_c_function(ctx, bitsy_draw_pixel, 3);
    duk_put_global_string(ctx, "bitsyDrawPixel");

    duk_push_c_function(ctx, bitsy_draw_tile, 3);
    duk_put_global_string(ctx, "bitsyDrawTile");

    duk_push_c_function(ctx, bitsy_draw_textbox, 2);
    duk_put_global_string(ctx, "bitsyDrawTextbox");

    duk_push_c_function(ctx, bitsy_clear, 1);
    duk_put_global_string(ctx, "bitsyClear");

    duk_push_c_function(ctx, bitsy_add_tile, 0);
    duk_put_global_string(ctx, "bitsyAddTile");

    duk_push_c_function(ctx, bitsy_reset_tiles, 0);
    duk_put_global_string(ctx, "bitsyResetTiles");

    duk_push_c_function(ctx, bitsy_set_textbox_size, 2);
    duk_put_global_string(ctx, "bitsySetTextboxSize");

    duk_push_c_function(ctx, bitsy_on_load, 1);
    duk_put_global_string(ctx, "bitsyOnLoad");

    duk_push_c_function(ctx, bitsy_on_quit, 1);
    duk_put_global_string(ctx, "bitsyOnQuit");

    duk_push_c_function(ctx, bitsy_on_update, 1);
    duk_put_global_string(ctx, "bitsyOnUpdate");
}
