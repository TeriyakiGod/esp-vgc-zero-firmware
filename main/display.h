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

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t vgc_lcd_io_handle = NULL;
static esp_lcd_panel_handle_t vgc_lcd_panel_handle = NULL;

/* LVGL display and touch */
static lv_display_t *vgc_display = NULL;

esp_err_t vgc_lcd_init();
esp_err_t vgc_lvgl_init();
esp_err_t vgc_lvgl_deinit();

void vgc_display_test();

#endif // DISPLAY_H