#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "bitsybox.h"

struct soundChannel {
    int duration;
    int frequency;
    int volume;
    int pulse;
} sound_channel;

// Timer configuration function for PWM generation
void configure_timer_for_pwm() {
    timer_config_t config = {
        .divider = 80,              // Divide 80 MHz APB clock to get 1 MHz timer tick
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
}