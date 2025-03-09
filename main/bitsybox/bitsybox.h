#ifndef BITSYBOX_H
#define BITSYBOX_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <duktape.h>

// #define DEMO_MODE
// #define TUNE_TOOL_MODE
#define ENABLE_BITSY_LOG

#define MEMORY_BLOCK_MAX 1024
#define PALETTE_MAX 256
#define TEXTURE_MAX MEMORY_BLOCK_MAX

// memory blocks
#define BITSY_VIDEO 0
#define BITSY_TEXTBOX 1
#define BITSY_MAP1 2
#define BITSY_MAP2 3
#define BITSY_SOUND1 4
#define BITSY_SOUND2 5

#define BITSY_TILE_START 6

// samples per second
#define AUDIO_SAMPLE_RATE 44100
// size of the audio buffer in samples (filled by audio callback)
#define AUDIO_BUFFER_SIZE 256

// graphics modes
#define BITSY_GFX_VIDEO 0
#define BITSY_GFX_MAP 1

// text modes
#define BITSY_TXT_HIREZ 0
#define BITSY_TXT_LOREZ 1

// size
#define BITSY_TILE_SIZE 8
#define BITSY_MAP_SIZE 16
#define BITSY_VIDEO_SIZE 128
#define RENDER_SCALE 1
#define TEXTBOX_RENDER_SCALE 1

// button codes
#define BITSY_BTN_UP 0
#define BITSY_BTN_DOWN 1
#define BITSY_BTN_LEFT 2
#define BITSY_BTN_RIGHT 3
#define BITSY_BTN_OK 4
#define BITSY_BTN_MENU 5

// pulse waves
#define BITSY_PULSE_1_8 0
#define BITSY_PULSE_1_4 1
#define BITSY_PULSE_1_2 2

#define SCREEN_BUFFER_ID 0
#define TEXTBOX_BUFFER_ID 1

extern uint16_t systemPalette[PALETTE_MAX];
extern uint16_t *drawingBuffers[TEXTURE_MAX];

/* INPUT */
extern bool isButtonUp;
extern bool isButtonDown;
extern bool isButtonLeft;
extern bool isButtonRight;

void init_input_gpio(void);

/* bitsy Interface */
duk_ret_t bitsy_log(duk_context *ctx);
duk_ret_t bitsy_get_button(duk_context *ctx);
duk_ret_t bitsy_set_graphics_mode(duk_context *ctx);
duk_ret_t bitsy_set_color(duk_context *ctx);
duk_ret_t bitsy_reset_colors(duk_context *ctx);
duk_ret_t bitsy_draw_begin(duk_context *ctx);
duk_ret_t bitsy_draw_end(duk_context *ctx);
duk_ret_t bitsy_draw_pixel(duk_context *ctx);
duk_ret_t bitsy_draw_tile(duk_context *ctx);
duk_ret_t bitsy_draw_textbox(duk_context *ctx);
duk_ret_t bitsy_clear(duk_context *ctx);
duk_ret_t bitsy_add_tile(duk_context *ctx);
duk_ret_t bitsy_reset_tiles(duk_context *ctx);
duk_ret_t bitsy_set_textbox_size(duk_context *ctx);
duk_ret_t bitsy_on_load(duk_context *ctx);
duk_ret_t bitsy_on_quit(duk_context *ctx);
duk_ret_t bitsy_on_update(duk_context *ctx);
void duk_register_bitsy_api(duk_context *ctx);

/* Duktape Interface */
void duk_init_bitsy_system(duk_context *ctx);
void duk_init_bitsy_game(duk_context *ctx);
void duk_update_game_state(duk_context *ctx);
void duk_quit_bitsy_game(duk_context *ctx);
void duk_deinit_bitsy_system(void);
void duk_destroy_heap(duk_context *ctx);
duk_context *duk_init_duktape_heap(void);


/* TASK */
void vBitsyEngineTask(void *pvParameters);

#endif // BITSYBOX_H