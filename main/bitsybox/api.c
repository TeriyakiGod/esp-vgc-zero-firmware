#include "bitsybox.h"

static const char *TAG = "BitsyAPI";

static int curGraphicsMode = 0;
static int curTextMode = 0;
lv_color_t systemPalette[SYSTEM_PALETTE_MAX];
lv_color_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];
static int curBufferId = -1;
static int tileStartBufferId = 2;
static int nextBufferId = 2;
static bool isTextBoxVisible = false;
static int textboxPosX = 0;
static int textboxPosY = 0;
static int textboxWidth = 104;
static int textboxHeight = 38;

// Log function to print messages
duk_ret_t native_log(duk_context *ctx) {
    const char *message = duk_safe_to_string(ctx, 0);
    ESP_LOGI(TAG, "Bitsy Log: %s", message);
    return 0;
}

// bitsy.button(code)
duk_ret_t native_buttonPressed(duk_context *ctx) {
    int buttonCode = duk_get_int(ctx, 0);
    bool isPressed = false;

    // Check button state
    switch (buttonCode) {
        case 0: isPressed = isButtonUp; break; // Up
        case 1: isPressed = isButtonDown; break; // Down
        case 2: isPressed = isButtonLeft; break; // Left
        case 3: isPressed = isButtonRight; break; // Right
        case 4: isPressed = isButtonDown || isButtonUp || isButtonLeft || isButtonRight; break; // Any
        case 5: isPressed = isButtonMenu; break; // Menu
        default: isPressed = false; break;
    }

    duk_push_boolean(ctx, isPressed);
    return 1;
}

// bitsy.getGameData()
// bitsy.getFontData()

// bitsy.graphicsMode(mode)
duk_ret_t native_setGraphicsMode(duk_context *ctx) {
    curGraphicsMode = duk_get_int(ctx, 0);
    ESP_LOGI(TAG, "Graphics mode set to %d", curGraphicsMode);
    return 0;
}

// bitsy.graphicsMode()
duk_ret_t native_getGraphicsMode(duk_context *ctx) {
    duk_push_int(ctx, curGraphicsMode);
    return 1;
}

// bitsy.textMode(mode)
duk_ret_t native_setTextMode(duk_context *ctx) {
    curTextMode = duk_get_int(ctx, 0);
    ESP_LOGI(TAG, "Graphics mode set to text mode");
    return 0;
}

// bitsy.textMode()
duk_ret_t native_getTextMode(duk_context *ctx) {
    duk_push_int(ctx, curTextMode);
    return 1;
}

// bitsy.color(color, r, g, b)
duk_ret_t native_setColor(duk_context *ctx) {
    int colorIndex = duk_get_int(ctx, 0);
    int r = duk_get_int(ctx, 1);
    int g = duk_get_int(ctx, 2);
    int b = duk_get_int(ctx, 3);
    
    // Assuming a simple RGB struct, adjust accordingly
    systemPalette[colorIndex] = lv_color_make(r, g, b);
    ESP_LOGI(TAG, "Set color %d to RGB(%d, %d, %d)", colorIndex, r, g, b);
    return 0;
}

// bitsy.tile()
duk_ret_t native_allocateTile(duk_context *ctx) {
    if (nextBufferId >= SYSTEM_DRAWING_BUFFER_MAX)
    {
        return 0;
    }

    // allocate a new tile buffer
    drawingBuffers[nextBufferId] = heap_caps_malloc(TILE_SIZE * TILE_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!drawingBuffers[nextBufferId])
    {
        ESP_LOGE(TAG, "Failed to allocate memory for tile buffer");
    }
    ESP_LOGI(TAG, "Allocated memory for tile buffer %d", nextBufferId);

    duk_push_int(ctx, nextBufferId);

    nextBufferId++;

    return 1;
}

// bitsy.delete(tile)
duk_ret_t native_deleteTile(duk_context *ctx) {
    int tileId = duk_get_int(ctx, 0);

    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }

    // free the tile buffer
    heap_caps_free(drawingBuffers[tileId]);
    drawingBuffers[tileId] = NULL;

    ESP_LOGI(TAG, "Deleted tile buffer %d", tileId);

    return 0;
}

// bitsy.fill(block, value)
duk_ret_t native_fillBlock(duk_context *ctx) {
    int tileId = duk_get_int(ctx, 0);
    int value = duk_get_int(ctx, 1);

    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }

    // Use memset to fill the memory with the value
    memset(drawingBuffers[tileId], value, TILE_SIZE * TILE_SIZE * sizeof(lv_color_t));

    return 0;
}

// bitsy.set(block, index, value)
duk_ret_t native_setBlockValue(duk_context *ctx) {
    int tileId = duk_get_int(ctx, 0);
    int index = duk_get_int(ctx, 1);
    int value = duk_get_int(ctx, 2);

    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }

    lv_color_t color = systemPalette[value];
    drawingBuffers[tileId][index] = color;

    return 0;
}

//bitsy.textbox(visible, x, y, w, h)
duk_ret_t native_configureTextbox(duk_context *ctx) {
    int visible = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
    int y = duk_get_int(ctx, 2);
    int w = duk_get_int(ctx, 3);
    int h = duk_get_int(ctx, 4);

    isTextBoxVisible = visible;
    textboxPosX = x;
    textboxPosY = y;

    if (w == textboxWidth && h == textboxHeight) {
        // No change in textbox size
        return 0;
    }
    
    textboxWidth = w;
    textboxHeight = h;

    // Free the old buffer if it exists to avoid memory leaks
    if (drawingBuffers[TEXTBOX_BUFFER_ID] != NULL) {
        heap_caps_free(drawingBuffers[TEXTBOX_BUFFER_ID]);
    }

    // Allocate new buffer based on the new textbox size and scale
    int bufferSize = textboxWidth * TEXTBOX_RENDER_SCALE * textboxHeight * TEXTBOX_RENDER_SCALE * sizeof(lv_color_t);
    drawingBuffers[TEXTBOX_BUFFER_ID] = (lv_color_t*) heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);

    if (drawingBuffers[TEXTBOX_BUFFER_ID] == NULL) {
        // Handle allocation failure
        return DUK_RET_ERROR;  // Return an error if memory allocation fails
    }

    // Clear the new buffer (initialize with some color, or leave as zero)
    memset(drawingBuffers[TEXTBOX_BUFFER_ID], 0, bufferSize);

    ESP_LOGI(TAG, "Textbox configuration: visible=%d, x=%d, y=%d, w=%d, h=%d", visible, x, y, w, h);

    return 0;
}

// bitsy.sound(channel. duration, frequency, volume, pulse)


void register_bitsy_api(duk_context *ctx)
{
    // Register the Bitsy API functions
    duk_push_global_object(ctx);

    duk_push_c_function(ctx, native_log, 1);
    duk_put_prop_string(ctx, -2, "native_log");

    duk_push_c_function(ctx, native_buttonPressed, 1);
    duk_put_prop_string(ctx, -2, "native_buttonPressed");

    duk_push_c_function(ctx, native_setGraphicsMode, 1);
    duk_put_prop_string(ctx, -2, "native_setGraphicsMode");

    duk_push_c_function(ctx, native_getGraphicsMode, 0);
    duk_put_prop_string(ctx, -2, "native_getGraphicsMode");

    duk_push_c_function(ctx, native_setTextMode, 1);
    duk_put_prop_string(ctx, -2, "native_setTextMode");

    duk_push_c_function(ctx, native_getTextMode, 0);
    duk_put_prop_string(ctx, -2, "native_getTextMode");

    duk_push_c_function(ctx, native_setColor, 4);
    duk_put_prop_string(ctx, -2, "native_setColor");

    duk_push_c_function(ctx, native_allocateTile, 0);
    duk_put_prop_string(ctx, -2, "native_allocateTile");

    duk_push_c_function(ctx, native_deleteTile, 1);
    duk_put_prop_string(ctx, -2, "native_deleteTile");

    duk_push_c_function(ctx, native_fillBlock, 2);
    duk_put_prop_string(ctx, -2, "native_fillBlock");

    duk_push_c_function(ctx, native_setBlockValue, 3);
    duk_put_prop_string(ctx, -2, "native_setBlockValue");

    duk_push_c_function(ctx, native_configureTextbox, 5);
    duk_put_prop_string(ctx, -2, "native_configureTextbox");

    // duk_push_c_function(ctx, native_setSound, 5);
    // duk_put_prop_string(ctx, -2, "native_setSound");

    // duk_push_c_function(ctx, native_setFrequency, 2);
    // duk_put_prop_string(ctx, -2, "native_setFrequency");

    // duk_push_c_function(ctx, native_setVolume, 2);
    // duk_put_prop_string(ctx, -2, "native_setVolume");

    // duk_push_c_function(ctx, native_setLoopFunction, 1);
    // duk_put_prop_string(ctx, -2, "native_setLoopFunction");

    duk_pop(ctx);
}