#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"

/* LCD size */
#define VGC_LCD_H_RES (128)
#define VGC_LCD_V_RES (128)

/* LCD settings */
#define VGC_LCD_SPI_NUM (SPI2_HOST)
#define VGC_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
#define VGC_LCD_CMD_BITS (8)
#define VGC_LCD_PARAM_BITS (8)
#define VGC_LCD_ELEMENT_ORDER (LCD_RGB_ELEMENT_ORDER_BGR)
#define VGC_LCD_BITS_PER_PIXEL (16)
#define VGC_LCD_DRAW_BUFF_DOUBLE (1)
#define VGC_LCD_DRAW_BUFF_HEIGHT (50)
#define VGC_LCD_BL_ON_LEVEL (1)

/* LCD pins */
#define VGC_LCD_GPIO_SCLK (GPIO_NUM_18)
#define VGC_LCD_GPIO_MOSI (GPIO_NUM_23)
#define VGC_LCD_GPIO_RST (GPIO_NUM_22)
#define VGC_LCD_GPIO_DC (GPIO_NUM_19)
#define VGC_LCD_GPIO_CS (GPIO_NUM_5)
#define VGC_LCD_GPIO_BL (GPIO_NUM_25)

esp_err_t vgc_lcd_clear();

esp_err_t vgc_lcd_init();
esp_err_t vgc_lcd_deinit();
void vgc_lcd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

esp_err_t vgc_lvgl_init();
esp_err_t vgc_lvgl_deinit();

#endif // DISPLAY_H