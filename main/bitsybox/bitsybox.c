#include "bitsybox.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "display.h"
#include "perfmon.h"

static const char *TAG = "BitsyBox";

void log_performance(const char *function_name, int64_t start_time, int64_t end_time)
{
    float time_ms = (end_time - start_time) / 1000.0; // Convert microseconds to milliseconds
    printf("%s took %.2f ms\n", function_name, time_ms);
}

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
    const TickType_t xFrequency = 16; // delay for mS
    for (;;)
    {
        int64_t start_time = esp_timer_get_time();
        
        duk_update_game_state(ctx);
        int64_t update_time = esp_timer_get_time();
        
        ESP_ERROR_CHECK(vgc_lcd_draw_bitmap(0, 0, SCREEN_SIZE, SCREEN_SIZE, drawingBuffers[SCREEN_BUFFER_ID]));
        int64_t draw_time = esp_timer_get_time();
        
        log_performance("Update Game State", start_time, update_time);
        log_performance("Draw Bitmap", update_time, draw_time);
        
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

    // Perform cleanup
    duk_quit_bitsy_game(ctx);
    duk_deinit_bitsy_system();
    duk_destroy_heap(ctx);

    vTaskDelete(NULL);
}
