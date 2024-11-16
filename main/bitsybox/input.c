#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"

#define BUTTON_UP 35
#define BUTTON_DOWN 34
#define BUTTON_LEFT 39
#define BUTTON_RIGHT 36

#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "INPUT";

// Button states
volatile bool isButtonUp = false;
volatile bool isButtonDown = false;
volatile bool isButtonLeft = false;
volatile bool isButtonRight = false;

// Button-specific handlers
static void IRAM_ATTR button_up_isr_handler(void *arg)
{
    isButtonUp = gpio_get_level(BUTTON_UP);
    ESP_EARLY_LOGI(TAG, "Button UP: %s", isButtonUp ? "Pressed" : "Released");
}

static void IRAM_ATTR button_down_isr_handler(void *arg)
{
    isButtonDown = gpio_get_level(BUTTON_DOWN);
    ESP_EARLY_LOGI(TAG, "Button DOWN: %s", isButtonDown ? "Pressed" : "Released");
}

static void IRAM_ATTR button_left_isr_handler(void *arg)
{
    isButtonLeft = gpio_get_level(BUTTON_LEFT);
    ESP_EARLY_LOGI(TAG, "Button LEFT: %s", isButtonLeft ? "Pressed" : "Released");
}

static void IRAM_ATTR button_right_isr_handler(void *arg)
{
    isButtonRight = gpio_get_level(BUTTON_RIGHT);
    ESP_EARLY_LOGI(TAG, "Button RIGHT: %s", isButtonRight ? "Pressed" : "Released");
}

// Initialize GPIO pins and attach interrupt handlers
void init_input_gpio(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;     // Interrupt on both rising and falling edges
    io_conf.mode = GPIO_MODE_INPUT;            // Set as input mode
    io_conf.pin_bit_mask = (1ULL << BUTTON_UP) | (1ULL << BUTTON_DOWN) |
                           (1ULL << BUTTON_LEFT) | (1ULL << BUTTON_RIGHT); // Select GPIO pins
    io_conf.pull_down_en = 1;                  // Enable pull-down mode
    io_conf.pull_up_en = 0;                    // Disable pull-up mode
    gpio_config(&io_conf);

    // Install GPIO ISR service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // Hook separate ISR handlers for each button
    gpio_isr_handler_add(BUTTON_UP, button_up_isr_handler, NULL);
    gpio_isr_handler_add(BUTTON_DOWN, button_down_isr_handler, NULL);
    gpio_isr_handler_add(BUTTON_LEFT, button_left_isr_handler, NULL);
    gpio_isr_handler_add(BUTTON_RIGHT, button_right_isr_handler, NULL);
}