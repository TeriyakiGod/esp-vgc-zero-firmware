#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"

/* LCD size */
#define EXAMPLE_LCD_H_RES (128)
#define EXAMPLE_LCD_V_RES (128)

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM (SPI2_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS (8)
#define EXAMPLE_LCD_PARAM_BITS (8)
#define EXAMPLE_LCD_ELEMENT_ORDER (LCD_RGB_ELEMENT_ORDER_BGR)
#define EXAMPLE_LCD_BITS_PER_PIXEL (16)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_BL_ON_LEVEL (1)

/* LCD pins */
#define EXAMPLE_LCD_GPIO_SCLK (GPIO_NUM_18)
#define EXAMPLE_LCD_GPIO_MOSI (GPIO_NUM_23)
#define EXAMPLE_LCD_GPIO_RST (GPIO_NUM_22)
#define EXAMPLE_LCD_GPIO_DC (GPIO_NUM_19)
#define EXAMPLE_LCD_GPIO_CS (GPIO_NUM_5)
#define EXAMPLE_LCD_GPIO_BL (GPIO_NUM_25)

esp_err_t vgc_lcd_clear();

esp_err_t vgc_lcd_init();
esp_err_t vgc_lcd_deinit();
esp_err_t vgc_lcd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

esp_err_t vgc_lvgl_init();
esp_err_t vgc_lvgl_deinit();

void vgc_esp_lcd_test();
void vgc_lvgl_hello_world();
void vgc_lvgl_color_test();

#endif // DISPLAY_H