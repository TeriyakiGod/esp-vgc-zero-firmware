#include "fs.h"
#include "display.h"
#include "bitsybox.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "VGC";

void app_main(void)
{
    // Init FS
    ESP_ERROR_CHECK(vgc_fs_init());

    /* LCD HW initialization */
    ESP_ERROR_CHECK(vgc_lcd_init());

    /* LVGL initialization */
    ESP_ERROR_CHECK(vgc_lvgl_init());

    // Run bitsy engine
    //app_duktape_bitsy();
    vgc_display_test();

    // Wait
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Deinit LVGL
    ESP_ERROR_CHECK(vgc_lvgl_deinit());

    // Deinit FS
    ESP_ERROR_CHECK(vgc_fs_deinit());

    ESP_LOGI(TAG, "Program completed.");
}
