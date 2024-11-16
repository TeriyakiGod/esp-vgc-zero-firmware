#include "display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_st7735.h"
#include "lvgl.h"

static const char *TAG = "LCD";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t vgc_lcd_io_handle = NULL;
static esp_lcd_panel_handle_t vgc_lcd_panel_handle = NULL;

/* LVGL display and touch */
static lv_display_t *vgc_display = NULL;

esp_err_t vgc_lcd_clear(){
    // fill screen with black
    uint16_t *black_bitmap = heap_caps_malloc(VGC_LCD_H_RES * VGC_LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    memset(black_bitmap, 0, VGC_LCD_H_RES * VGC_LCD_V_RES * sizeof(uint16_t));
    esp_err_t result = esp_lcd_panel_draw_bitmap(vgc_lcd_panel_handle, 0, 0, VGC_LCD_H_RES, VGC_LCD_V_RES, black_bitmap);
    heap_caps_free(black_bitmap);
    return result;
}

esp_err_t vgc_lcd_init()
{
    esp_err_t ret = ESP_OK;

    /* LCD backlight */
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << VGC_LCD_GPIO_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    /* LCD initialization */
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = VGC_LCD_GPIO_SCLK,
        .mosi_io_num = VGC_LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = VGC_LCD_H_RES * VGC_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(VGC_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = VGC_LCD_GPIO_DC,
        .cs_gpio_num = VGC_LCD_GPIO_CS,
        .pclk_hz = VGC_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = VGC_LCD_CMD_BITS,
        .lcd_param_bits = VGC_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)VGC_LCD_SPI_NUM, &io_config, &vgc_lcd_io_handle), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = VGC_LCD_GPIO_RST,
        .rgb_ele_order = VGC_LCD_ELEMENT_ORDER,
        .bits_per_pixel = VGC_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7735(vgc_lcd_io_handle, &panel_config, &vgc_lcd_panel_handle), err, TAG, "New panel failed");

    esp_lcd_panel_reset(vgc_lcd_panel_handle);
    esp_lcd_panel_init(vgc_lcd_panel_handle);
    esp_lcd_panel_mirror(vgc_lcd_panel_handle, true, true);
    esp_lcd_panel_set_gap(vgc_lcd_panel_handle, 2, 3);
    esp_lcd_panel_invert_color(vgc_lcd_panel_handle, false);
    esp_lcd_panel_disp_on_off(vgc_lcd_panel_handle, true);
    vgc_lcd_clear();

    /* LCD backlight on */
    ESP_ERROR_CHECK(gpio_set_level(VGC_LCD_GPIO_BL, VGC_LCD_BL_ON_LEVEL));

    return ret;

err:
    if (vgc_lcd_panel_handle)
    {
        esp_lcd_panel_del(vgc_lcd_panel_handle);
    }
    if (vgc_lcd_io_handle)
    {
        esp_lcd_panel_io_del(vgc_lcd_io_handle);
    }
    spi_bus_free(VGC_LCD_SPI_NUM);
    return ret;
}

esp_err_t vgc_lvgl_init()
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 4096,       /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = vgc_lcd_io_handle,
        .panel_handle = vgc_lcd_panel_handle,
        .buffer_size = VGC_LCD_H_RES * VGC_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = VGC_LCD_DRAW_BUFF_DOUBLE,
        .hres = VGC_LCD_H_RES,
        .vres = VGC_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        }};
    vgc_display = lvgl_port_add_disp(&disp_cfg);
    
    return ESP_OK;
}

esp_err_t vgc_lcd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    return esp_lcd_panel_draw_bitmap(vgc_lcd_panel_handle, x, y, w, h, bitmap);
}

esp_err_t vgc_lcd_deinit()
{
    esp_err_t ret = ESP_OK;

    /* LCD backlight off */
    ESP_ERROR_CHECK(gpio_set_level(VGC_LCD_GPIO_BL, !VGC_LCD_BL_ON_LEVEL));

    /* Deinitialize LCD */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(vgc_lcd_panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_del(vgc_lcd_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_io_del(vgc_lcd_io_handle));
    ESP_ERROR_CHECK(spi_bus_free(VGC_LCD_SPI_NUM));

    return ret;
}

esp_err_t vgc_lvgl_deinit()
{
    return lvgl_port_remove_disp(vgc_display);
}