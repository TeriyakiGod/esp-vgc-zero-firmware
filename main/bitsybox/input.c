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
bool isButtonW = false;
bool isButtonA = false;
bool isButtonS = false;
bool isButtonD = false;
bool isButtonR = false;
bool isButtonSpace = false;
bool isButtonReturn = false;
bool isButtonEscape = false;
bool isButtonLCtrl = false;
bool isButtonRCtrl = false;
bool isButtonLAlt = false;
bool isButtonRAlt = false;
bool isButtonPadUp = false;
bool isButtonPadDown = false;
bool isButtonPadLeft = false;
bool isButtonPadRight = false;
bool isButtonPadA = false;
bool isButtonPadB = false;
bool isButtonPadX = false;
bool isButtonPadY = false;
bool isButtonPadStart = false;

// Interrupt handler
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;

    switch (gpio_num) {
        case 2:
            isButtonUp = !isButtonUp;  // Toggle state
            break;
        case 4:
            isButtonDown = !isButtonDown;  // Toggle state
            break;
        case 12:
            isButtonLeft = !isButtonLeft;  // Toggle state
            break;
        case 13:
            isButtonRight = !isButtonRight;  // Toggle state
            break;
        default:
            break;
    }
}

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

// Install and configure GPIO interrupts
void init_input_interrupts(void) {
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
    // Attach interrupts for each GPIO
    gpio_isr_handler_add(2, gpio_isr_handler, (void*) 2);
    gpio_isr_handler_add(4, gpio_isr_handler, (void*) 4);
    gpio_isr_handler_add(12, gpio_isr_handler, (void*) 12);
    gpio_isr_handler_add(13, gpio_isr_handler, (void*) 13);

    ESP_LOGI(TAG, "GPIO interrupts installed.");
}

void init_input(void) {
    init_input_gpio();
    init_input_interrupts();
}