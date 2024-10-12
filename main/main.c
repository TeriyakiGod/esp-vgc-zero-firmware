#include "fs.h"
#include "display.h"
#include "bitsybox/bitsybox.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bitsybox/test.h"

static const char *TAG = "VGC";

void app_main(void)
{
    // Init FS
    ESP_ERROR_CHECK(vgc_fs_init());

    /* LCD HW initialization */
    ESP_ERROR_CHECK(vgc_lcd_init());

    /* LVGL initialization */
    ESP_ERROR_CHECK(vgc_lvgl_init());

    //vgc_esp_lcd_test();
    app_duktape_bitsy();

    // Deinit LCD
    ESP_ERROR_CHECK(vgc_lcd_deinit());

    // Deinit LVGL
    ESP_ERROR_CHECK(vgc_lvgl_deinit());

    // Deinit FS
    ESP_ERROR_CHECK(vgc_fs_deinit());
}
