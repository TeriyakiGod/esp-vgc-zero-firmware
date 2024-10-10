#ifndef BITSYBOX_H
#define BITSYBOX_H

#include <stdint.h>
#include <duktape.h>
#include <stdbool.h>

#define SYSTEM_PALETTE_MAX 256
#define SYSTEM_DRAWING_BUFFER_MAX 1024

#define SCREEN_SIZE 128
#define TILE_SIZE 8
#define ROOM_SIZE 16
#define RENDER_SCALE 1
#define TEXTBOX_RENDER_SCALE 1

#define SCREEN_BUFFER_ID 0
#define TEXTBOX_BUFFER_ID 1

extern uint16_t systemPalette[SYSTEM_PALETTE_MAX];
extern uint16_t *drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

/* INPUT */
extern bool isButtonUp;
extern bool isButtonDown;
extern bool isButtonLeft;
extern bool isButtonRight;
extern bool isButtonW;
extern bool isButtonA;
extern bool isButtonS;
extern bool isButtonD;
extern bool isButtonR;
extern bool isButtonSpace;
extern bool isButtonReturn;
extern bool isButtonEscape;
extern bool isButtonLCtrl;
extern bool isButtonRCtrl;
extern bool isButtonLAlt;
extern bool isButtonRAlt;
extern bool isButtonPadUp;
extern bool isButtonPadDown;
extern bool isButtonPadLeft;
extern bool isButtonPadRight;
extern bool isButtonPadA;
extern bool isButtonPadB;
extern bool isButtonPadX;
extern bool isButtonPadY;
extern bool isButtonPadStart;

void init_input(void);

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