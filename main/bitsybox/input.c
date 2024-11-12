#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bitsybox.h"

#define GPIO_INPUT_PIN_SEL  ((1ULL<<2) | (1ULL<<4) | (1ULL<<12) | (1ULL<<13))  // Pins 2, 4, 12, 13
#define ESP_INTR_FLAG_DEFAULT 0

static const char* TAG = "INPUT";

// Button states
bool isButtonUp = false;
bool isButtonDown = false;
bool isButtonLeft = false;
bool isButtonRight = false;
bool isButtonMenu = false;

// Initialize GPIO input pins
void init_input_gpio(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Interrupt on rising edge
    io_conf.mode = GPIO_MODE_INPUT;         // Set as input mode
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;  // Select GPIO pins
    io_conf.pull_down_en = 1;               // Enable pull-down mode
    io_conf.pull_up_en = 0;                 // Disable pull-up mode
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "GPIO inputs configured with pull-down.");
}

void get_input(void){
    isButtonUp = gpio_get_level(2);
    isButtonDown = gpio_get_level(4);
    isButtonLeft = gpio_get_level(12);
    isButtonRight = gpio_get_level(13);
}

void init_input(void) {
    init_input_gpio();
}