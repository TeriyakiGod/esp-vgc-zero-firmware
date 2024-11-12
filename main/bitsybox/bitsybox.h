#ifndef BITSYBOX_H
#define BITSYBOX_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>
#include <duktape.h>

#define GAME_DATA_FILE_PATH "/spiflash/bitsy/games/mossland.bitsy"
#define FONT_FILE_PATH "/spiflash/bitsy/font/ascii_small.bitsyfont"

#define SYSTEM_PALETTE_MAX 256
#define SYSTEM_DRAWING_BUFFER_MAX 1024

#define SCREEN_SIZE 128
#define TILE_SIZE 8
#define ROOM_SIZE 16
#define RENDER_SCALE 1
#define TEXTBOX_RENDER_SCALE 1

#define SCREEN_BUFFER_ID 0
#define TEXTBOX_BUFFER_ID 1

#define MAX_VOLUME 15
#define BASE_GAIN 0.01
#define BASE_FREQUENCY 100  // Base frequency in Hz

// VIDEO 
extern lv_color_t systemPalette[SYSTEM_PALETTE_MAX];
extern lv_color_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

/* INPUT */
extern bool isButtonUp;
extern bool isButtonDown;
extern bool isButtonLeft;
extern bool isButtonRight;
extern bool isButtonMenu;

void init_input(void);
void get_input(void);

/* API */
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
void register_bitsy_api(duk_context *ctx);

/* APP */
void app_duktape_bitsy();

#endif // BITSYBOX_H