#include "bitsybox.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "display.h"
#include "perfmon.h"
#include "esp_lcd_panel_io.h"
#include "esp_attr.h"

static const char *TAG = "BitsyBox";

void vBitsyEngineTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Bitsy Engine Task");
    // Initialize Duktape heap
    duk_context *ctx = duk_init_duktape_heap();
    // Initialize Bitsy system
    duk_init_bitsy_system(ctx);
    // Initialize Bitsy game
    duk_init_bitsy_game(ctx);
    // Initialize input GPIO
    init_input_gpio();
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(16);
    for (;;)
    {
        int64_t start_time = esp_timer_get_time();

        duk_update_game_state(ctx);
        int64_t update_time = esp_timer_get_time();
        vgc_lcd_draw_bitmap(0, 0, SCREEN_SIZE, SCREEN_SIZE, drawingBuffers[SCREEN_BUFFER_ID]);
        int64_t draw_time = esp_timer_get_time();

        ESP_LOGI(TAG, "Update Time: %lld ms, Draw Time: %lld ms", (update_time - start_time) / 1000, (draw_time - update_time) / 1000);

        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    // // Perform cleanup
    // duk_quit_bitsy_game(ctx);
    // duk_deinit_bitsy_system();
    // duk_destroy_heap(ctx);
    // vTaskDelete(NULL);
}
