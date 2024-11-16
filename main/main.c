#include "fs.h"
#include "display.h"
#include "bitsybox/bitsybox.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "VGC";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BitsyBox");
    // Init FS
    ESP_ERROR_CHECK(vgc_fs_init());
    /* LCD HW initialization */
    ESP_ERROR_CHECK(vgc_lcd_init());
    // Start BitsyBox
    xTaskCreatePinnedToCore(vBitsyEngineTask, "BitsyEngineTask", 12 * 1024, NULL, 5, NULL, 1);
    return;
}