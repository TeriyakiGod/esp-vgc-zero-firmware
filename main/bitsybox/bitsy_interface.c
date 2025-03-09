#include "bitsybox.h"
#include "esp_attr.h"

static const char *TAG = "BitsyAPI";

/* GLOBALS */
int curGraphicsMode = 0;
int curTextMode = 0;

// render scales
int renderScale = 1;
int textboxRenderScale = 1;

int shouldRenderTextures = 0;

// textbox state
int isTextboxVisible = 0;
int textboxX = 0;
int textboxY = 0;
int textboxWidth = 0;
int textboxHeight = 0;

/* # MEMORY */

typedef struct MemoryBlock {
	uint32_t size;
	uint8_t* data;
} MemoryBlock;

MemoryBlock memory[MEMORY_BLOCK_MAX];

void freeMemoryBlock(int block) {
	if (memory[block].data != NULL) {
		free(memory[block].data);
	}

	memory[block].size = -1;
	memory[block].data = NULL;
}

void initializeMemoryBlocks() {
	for (int i = 0; i < MEMORY_BLOCK_MAX; i++) {
		freeMemoryBlock(i);
	}
}

void allocateMemoryBlock(int block, uint16_t size) {
	// free any existing memory before re-allocating it!
	freeMemoryBlock(block);

	memory[block].size = size;
	memory[block].data = calloc(size, sizeof(uint8_t));
}

int isMemoryBlockEmpty(int block) {
	return memory[block].size <= 0 || memory[block].data == NULL;
}

int isMemoryBlockValid(int block) {
	return block >= 0 && block < MEMORY_BLOCK_MAX && !isMemoryBlockEmpty(block);
}

duk_ret_t bitsy_log(duk_context *ctx)
{
    const char *printStr;
    printStr = duk_safe_to_string(ctx, 0);
    ESP_LOGI(TAG, "Bitsy: %s", printStr);
    return 0;
}

int audioStep = 0; // global audio step counter for sampling
float audioVolume = 0.0f; // global audio volume (range: 0.0 - 1.0)

typedef struct PulseWave {
	int cycle; // cycle length in sample steps
	int duty; // duty length in sample steps
} PulseWave;

PulseWave wave(float frequency, float dutyCycle) {
	// calculate cycle length in samples
	float cycle = AUDIO_SAMPLE_RATE / frequency;
	// calcualte duty lenght in samples
	float duty = cycle * dutyCycle;

	// convert cycle and duty to integer steps
	return (PulseWave) {
		.cycle = floor(cycle),
		.duty = floor(duty),
	};
}

int pulse(PulseWave* wave, int step) {
	return (step % wave->cycle) <= wave->duty ? 1 : 0;	
}

// sound channels
PulseWave soundChannel1;
float volumeChannel1 = 0.0f; // volume from 0.0 - 1.0
int durationChannel1 = 0; // duration in *samples* (not frames or ms)
int dutyChannel1;

PulseWave soundChannel2;
float volumeChannel2 = 0.0f; // volume from 0.0 - 1.0
int durationChannel2 = 0; // duration in *samples* (not frames or ms)
int dutyChannel2;

void audioCallback(void* userdata, uint8_t* stream, int len) {
	float* fstream = (float*) stream;

	for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
		// increment global audio step
		audioStep++;

		// decrement channel duration counters and mute audio when they reach zero
		durationChannel1--;
		if (durationChannel1 <= 0) {
			volumeChannel1 = 0.0f;
			durationChannel1 = 0;
		}

		durationChannel2--;
		if (durationChannel2 <= 0) {
			volumeChannel2 = 0.0f;
			durationChannel2 = 0;
		}

		// calculate pulse wave sample for channel 1
		fstream[(i * 2) + 0] = pulse(&soundChannel1, audioStep) * volumeChannel1 * audioVolume;

		// calculate pulse wave sample for channel 2
		fstream[(i * 2) + 1] = pulse(&soundChannel2, audioStep) * volumeChannel2 * audioVolume;
	}
}

duk_ret_t bitsy_get_button(duk_context *ctx)
{
    int buttonCode = duk_get_int(ctx, 0);
    switch (buttonCode)
    {
    case 0: // UP
        duk_push_boolean(ctx, isButtonUp);
        break;
    case 1: // DOWN
        duk_push_boolean(ctx, isButtonDown);
        break;
    case 2: // LEFT
        duk_push_boolean(ctx, isButtonLeft);
        break;
    case 3: // RIGHT
        duk_push_boolean(ctx, isButtonRight);
        break;
    case 4: // SELECT
        duk_push_boolean(ctx, 0);
        break;
    case 5: // START
        duk_push_boolean(ctx, 0);
        break;
    default:
        duk_push_boolean(ctx, 0);
        break;
    }
    return 1;
}

duk_ret_t bitsy_get_gamedata(duk_context* ctx) {
	duk_peval_string(ctx, "__bitsybox_game_data__");

	return 1;
}

duk_ret_t bitsy_get_fontdata(duk_context* ctx) {
	duk_peval_string(ctx, "__bitsybox_default_font__");

	return 1;
}

duk_ret_t bitsyGraphicsMode(duk_context* ctx) {
	// set the graphics mode if there is an input mode
	if (duk_get_top(ctx) >= 1) {
		int prevGraphicsMode = curGraphicsMode;
		curGraphicsMode = duk_get_int(ctx, 0);

		if (curGraphicsMode != prevGraphicsMode) {
			shouldRenderTextures = 1;
		}
	}

	// return the current graphics mode
	duk_push_int(ctx, curGraphicsMode);

	return 1;
}

duk_ret_t bitsyTextMode(duk_context* ctx) {
	// set the text mode if there is an input mode
	if (duk_get_top(ctx) >= 1) {
		int prevTextMode =  curTextMode;
		curTextMode = duk_get_int(ctx, 0);

		// update the textbox render scale
		textboxRenderScale = (curTextMode == BITSY_TXT_LOREZ) ? 4 : 2;

		if (curTextMode != prevTextMode) {
			shouldRenderTextures = 1;
		}
	}

	// return the current text mode
	duk_push_int(ctx, curTextMode);

	return 1;
}

duk_ret_t bitsy_set_color(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    int r = duk_get_int(ctx, 1);
    int g = duk_get_int(ctx, 2);
    int b = duk_get_int(ctx, 3);

    // Pack the color into a uint16_t (RGB565 format)
    uint16_t color = ((r & 0xF8) << 8) |  // Top 5 bits of red
                     ((g & 0xFC) << 3) |  // Top 6 bits of green
                     ((b & 0xF8) >> 3);   // Top 5 bits of blue

    // Swap bytes if necessary
    color = (color >> 8) | (color << 8);

    systemPalette[paletteIndex] = color;
    shouldRenderTextures = 1;
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
    if (curBufferId == 0 && curGraphicsMode == 0)
    {
        // Use scaled coordinates
        for (int i = 0; i < RENDER_SCALE; i++)
        {
            for (int j = 0; j < RENDER_SCALE; j++)
            {
                drawingBuffers[SCREEN_BUFFER_ID][(scaledY + j) * SCREEN_SIZE + (scaledX + i)] = color;
            }
        }
    }
    else if (curBufferId == 1 && curGraphicsMode == 1)
    {
        // Use textboxRenderScale for this buffer
        int scaledTextboxX = x * TEXTBOX_RENDER_SCALE;
        int scaledTextboxY = y * TEXTBOX_RENDER_SCALE;
        for (int i = 0; i < TEXTBOX_RENDER_SCALE; i++)
        {
            for (int j = 0; j < TEXTBOX_RENDER_SCALE; j++)
            {
                drawingBuffers[TEXTBOX_BUFFER_ID][(scaledTextboxY + j) * textboxWidth + (scaledTextboxX + i)] = color;
            }
        }
    }
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId && curGraphicsMode == 1)
    {
        // Use scaled coordinates for tile buffer
        for (int i = 0; i < RENDER_SCALE; i++)
        {
            for (int j = 0; j < RENDER_SCALE; j++)
            {
                drawingBuffers[curBufferId][(scaledY + j) * TILE_SIZE + (scaledX + i)] = color;
            }
        }
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
    // Ensure the tileId is valid
    if (tileId < tileStartBufferId || tileId >= nextBufferId)
    {
        return 0;
    }
    // Calculate the tile position and size with render scale
    int scaledX = x * TILE_SIZE * RENDER_SCALE;
    int scaledY = y * TILE_SIZE * RENDER_SCALE;
    // int scaledTileSize = TILE_SIZE * RENDER_SCALE;
    // Iterate over each pixel of the tile and draw it to the screen buffer
    for (int ty = 0; ty < TILE_SIZE; ty++)
    {
        for (int tx = 0; tx < TILE_SIZE; tx++)
        {
            uint16_t color = drawingBuffers[tileId][ty * TILE_SIZE + tx]; // Get the pixel color from the tile buffer
            // Scale the pixel drawing using RENDER_SCALE
            for (int i = 0; i < RENDER_SCALE; i++)
            {
                for (int j = 0; j < RENDER_SCALE; j++)
                {
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

duk_ret_t bitsy_draw_textbox(duk_context *ctx)
{
    // Can only draw the textbox on the screen buffer in tile mode
    if (curBufferId != 0 || curGraphicsMode != 1)
    {
        return 0;
    }
    int x = duk_get_int(ctx, 0);
    int y = duk_get_int(ctx, 1);
    // Calculate the scaled position of the textbox
    int scaledX = x * TEXTBOX_RENDER_SCALE;
    int scaledY = y * TEXTBOX_RENDER_SCALE;
    // Iterate over each pixel of the textbox buffer and scale it
    for (int ty = 0; ty < textboxHeight; ty++)
    {
        for (int tx = 0; tx < textboxWidth; tx++)
        {
            uint16_t color = drawingBuffers[TEXTBOX_BUFFER_ID][ty * textboxWidth + tx]; // Get the pixel color from the textbox buffer
            // Scale the pixel drawing using TEXTBOX_RENDER_SCALE
            for (int i = 0; i < TEXTBOX_RENDER_SCALE; i++)
            {
                for (int j = 0; j < TEXTBOX_RENDER_SCALE; j++)
                {
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

duk_ret_t bitsy_clear(duk_context *ctx)
{
    int paletteIndex = duk_get_int(ctx, 0);
    uint16_t color = systemPalette[paletteIndex];
    // Clear the screen buffer
    if (curBufferId == 0)
    {
        for (int y = 0; y < SCREEN_SIZE * RENDER_SCALE; y++)
        {
            for (int x = 0; x < SCREEN_SIZE * RENDER_SCALE; x++)
            {
                drawingBuffers[SCREEN_BUFFER_ID][y * SCREEN_SIZE + x] = color;
            }
        }
    }
    // Clear the textbox buffer
    else if (curBufferId == 1)
    {
        for (int y = 0; y < textboxHeight * TEXTBOX_RENDER_SCALE; y++)
        {
            for (int x = 0; x < textboxWidth * TEXTBOX_RENDER_SCALE; x++)
            {
                drawingBuffers[TEXTBOX_BUFFER_ID][y * textboxWidth + x] = color;
            }
        }
    }
    // Clear the tile buffer
    else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId)
    {
        for (int y = 0; y < TILE_SIZE * RENDER_SCALE; y++)
        {
            for (int x = 0; x < TILE_SIZE * RENDER_SCALE; x++)
            {
                drawingBuffers[curBufferId][y * TILE_SIZE + x] = color;
            }
        }
    }
    return 0;
}

duk_ret_t bitsy_add_tile(duk_context *ctx)
{
    if (nextBufferId >= TEXTURE_MAX)
    {
        // todo : error handling?
        return 0;
    }
    // free memory if it exists
    if (drawingBuffers[nextBufferId] != NULL)
    {
        heap_caps_free(drawingBuffers[nextBufferId]);
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

duk_ret_t bitsy_set_textbox_size(duk_context *ctx)
{
    // Get the new textbox width and height from the context
    int newTextboxWidth = duk_get_int(ctx, 0);
    int newTextboxHeight = duk_get_int(ctx, 1);
    if (newTextboxWidth == textboxWidth && newTextboxHeight == textboxHeight)
    {
        // No change in textbox size
        return 0;
    }
    // Free the old buffer if it exists to avoid memory leaks
    if (drawingBuffers[TEXTBOX_BUFFER_ID] != NULL)
    {
        heap_caps_free(drawingBuffers[TEXTBOX_BUFFER_ID]);
    }
    // Allocate new buffer based on the new textbox size and scale
    int bufferSize = textboxWidth * TEXTBOX_RENDER_SCALE * textboxHeight * TEXTBOX_RENDER_SCALE * sizeof(uint16_t);
    drawingBuffers[TEXTBOX_BUFFER_ID] = (uint16_t *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    if (drawingBuffers[TEXTBOX_BUFFER_ID] == NULL)
    {
        // Handle allocation failure
        return DUK_RET_ERROR; // Return an error if memory allocation fails
    }
    // Clear the new buffer (initialize with some color, or leave as zero)
    memset(drawingBuffers[TEXTBOX_BUFFER_ID], 0, bufferSize);
    ESP_LOGI(TAG, "Set textbox size to %d x %d", textboxWidth, textboxHeight);
    return 0; // Return success
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

duk_ret_t IRAM_ATTR bitsy_on_update(duk_context *ctx)
{
    duk_put_global_string(ctx, "__bitsybox_on_update__");
    return 0;
}

void duk_register_bitsy_api(duk_context *ctx)
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
    ESP_LOGI(TAG, "Bitsy API registered");
}
