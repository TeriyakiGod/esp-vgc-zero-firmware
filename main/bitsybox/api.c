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

duk_ret_t bitsy_draw_pixel(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    uint16_t color = systemPalette[paletteIndex];

    if (curBufferId == 0 && curGraphicsMode == 0) {
		drawingBuffers[SCREEN_BUFFER_ID][y * SCREEN_SIZE + x] = color;
	}
	else if (curBufferId == 1 && curGraphicsMode == 1) {
		drawingBuffers[TEXTBOX_BUFFER_ID][y * textboxWidth + x] = color;
	}
	else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId && curGraphicsMode == 1) {
		drawingBuffers[curBufferId][y * TILE_SIZE + x] = color;
	}

    return 0;
}

duk_ret_t bitsy_draw_tile(duk_context *ctx)
{
    // Can only draw tiles on the screen buffer in tile mode
    if (curBufferId != 0 || curGraphicsMode != 1)
    {
        return 0;
    }

    int tileId = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);

    // Validate that tileId is within the allowed range
    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }

    // Calculate base offsets for both buffers
    uint16_t *tileBuffer = drawingBuffers[tileId];
    uint16_t *screenBuffer = drawingBuffers[SCREEN_BUFFER_ID];

    // Copy tile to screen buffer row by row
    for (int ty = 0; ty < TILE_SIZE; ty++)
    {
        // Precompute offsets for the current row in both buffers
        int screenRowOffset = (y + ty) * SCREEN_SIZE + x;
        int tileRowOffset = ty * TILE_SIZE;

        // Use memcpy to copy the entire row from the tile buffer to the screen buffer
        memcpy(&screenBuffer[screenRowOffset], &tileBuffer[tileRowOffset], TILE_SIZE * sizeof(uint16_t));
    }

    return 0;
}

duk_ret_t bitsy_draw_textbox(duk_context *ctx)
{
    if (curBufferId != 0 || curGraphicsMode != 1)
    {
        return 0;
    }

    int x = duk_get_int(ctx, 0);
    int y = duk_get_int(ctx, 1);

    // Calculate base offsets for both buffers
    uint16_t *textboxBuffer = drawingBuffers[TEXTBOX_BUFFER_ID];
    uint16_t *screenBuffer = drawingBuffers[SCREEN_BUFFER_ID];

    // Precompute the start row in the screen buffer
    for (int ty = 0; ty < textboxHeight; ty++)
    {
        // Precompute offsets for the current row in both buffers
        int screenRowOffset = (y + ty) * SCREEN_SIZE + x;
        int textboxRowOffset = ty * textboxWidth;

        // Use memcpy to copy an entire row at once
        memcpy(&screenBuffer[screenRowOffset], &textboxBuffer[textboxRowOffset], textboxWidth * sizeof(uint16_t));
    }

    return 0;
}

duk_ret_t bitsy_clear(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);

    uint16_t color = systemPalette[paletteIndex];

    if (curBufferId == 0)
    {
        // Clear the screen buffer
        for (int i = 0; i < SCREEN_SIZE * SCREEN_SIZE; i++)
        {
            drawingBuffers[SCREEN_BUFFER_ID][i] = color;
        }
    }
    else if (curBufferId == 1)
    {
        // Clear the textbox buffer
        for (int i = 0; i < textboxWidth * textboxHeight; i++)
        {
            drawingBuffers[TEXTBOX_BUFFER_ID][i] = color;
        }
    }
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId)
    {
        // Clear the tile buffer
        for (int i = 0; i < TILE_SIZE * TILE_SIZE; i++)
        {
            drawingBuffers[curBufferId][i] = color;
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

    duk_push_int(ctx, nextBufferId);

    nextBufferId++;

    return 1;
}

duk_ret_t bitsy_reset_tiles(duk_context *ctx)
{
    nextBufferId = tileStartBufferId;

    return 0;
}

duk_ret_t bitsy_set_textbox_size(duk_context *ctx)
{
    textboxWidth = duk_get_int(ctx, 0);
    textboxHeight = duk_get_int(ctx, 1);

    heap_caps_free(drawingBuffers[TEXTBOX_BUFFER_ID]);
    drawingBuffers[TEXTBOX_BUFFER_ID] = heap_caps_malloc(textboxWidth * textboxHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!drawingBuffers[TEXTBOX_BUFFER_ID])
    {
        ESP_LOGE(TAG, "Failed to allocate memory for textbox buffer");
    }

    return 0;
}

duk_ret_t bitsy_on_load(duk_context *ctx)
{
    // hacky to just stick it in the global namespace??
    duk_put_global_string(ctx, "__bitsybox_on_load__");

    return 0;
}

duk_ret_t bitsy_on_quit(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_quit__");

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
