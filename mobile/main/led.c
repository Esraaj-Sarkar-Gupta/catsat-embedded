#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "RGB_BLINK";

// Onboard WS2812 RGB LED GPIO for most ESP32-S3 boards
#define BLINK_GPIO 48

static led_strip_handle_t led_strip;

static void configure_led(void)
{
    ESP_LOGI(TAG, "Configuring WS2812 RGB LED on GPIO %d...", BLINK_GPIO);

    /* 1. LED Strip Hardware Config */
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    /* 2. RMT Peripheral Backend Config */
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    /* 3. Create LED Strip Handle */
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    /* Turn off LED initially */
    led_strip_clear(led_strip);
}

void app_main(void)
{
    configure_led();

    uint8_t color_step = 0;

    while (1) {
        switch (color_step) {
            case 0:
                ESP_LOGI(TAG, "Color: RED");
                led_strip_set_pixel(led_strip, 0, 40, 0, 0); // Red
                break;
            case 1:
                ESP_LOGI(TAG, "Color: GREEN");
                led_strip_set_pixel(led_strip, 0, 0, 40, 0); // Green
                break;
            case 2:
                ESP_LOGI(TAG, "Color: BLUE");
                led_strip_set_pixel(led_strip, 0, 0, 0, 40); // Blue
                break;
        }

        led_strip_refresh(led_strip);
        color_step = (color_step + 1) % 3;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}