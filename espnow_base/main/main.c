/* Symmetric two-board ESP-NOW demo. Flash the same image to both boards. */

#include <inttypes.h>

#include "command_parser.h"
#include "esp_err.h"
#include "esp_log.h"
#include "espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#define DISCOVERY_PERIOD_MS 5000
#define COMMAND_PERIOD_MS 2000
#define LED_PULSE_MS 80
#define LED_GPIO 48 /* ESP32-S3 dev boards with a WS2812 */
#define TELEMETRY_PERIOD_MS 250

static const char *TAG = "P2P_APP";
static led_strip_handle_t led_strip;

static void configure_led(void)
{
    const led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}

static void pulse_led(void)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 0, 32));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    vTaskDelay(pdMS_TO_TICKS(LED_PULSE_MS));
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}

static void process_message(const espnow_message_t *message)
{
    if (message->type == ESPNOW_MESSAGE_RESPONSE) {
        ESP_LOGI(TAG, "Response #%" PRIu32 ": %s", message->sequence, message->text);
        return;
    }
    if (message->type != ESPNOW_MESSAGE_COMMAND) {
        return;
    }

    char response[ESPNOW_TEXT_SIZE];
    command_parse(message->text, response, sizeof(response));
    ESP_LOGI(TAG, "Command '%s' -> '%s'", message->text, response);
    esp_err_t err = espnow_send(ESPNOW_MESSAGE_RESPONSE, message->sequence, response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not queue command response: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    configure_led();
    ESP_ERROR_CHECK(espnow_init());

    uint32_t discovery_sequence = 0;
    uint32_t command_sequence = 0;

    TickType_t last_discovery = 0;
    TickType_t last_command = 0;

    uint32_t telemetry_sequence = 0;
    TickType_t last_telemetry = 0;

    telemetry_data_t telemetry = {0};
    while (true) {
        const TickType_t now = xTaskGetTickCount();

        if (now - last_discovery >= pdMS_TO_TICKS(DISCOVERY_PERIOD_MS)) {
            esp_err_t err = espnow_send(ESPNOW_MESSAGE_DISCOVERY, discovery_sequence++, "HELLO");
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Sent discovery broadcast");
                pulse_led();
            } else {
                ESP_LOGE(TAG, "Discovery broadcast failed: %s", esp_err_to_name(err));
            }
            last_discovery = now;
        }

        if (espnow_peer_available() && now - last_telemetry >= pdMS_TO_TICKS(TELEMETRY_PERIOD_MS)) 
        {
            esp_err_t err = espnow_send_telemetry(
                &telemetry,
                telemetry_sequence
            );

            if (err == ESP_OK) {
                telemetry_sequence++;
                ESP_LOGI(TAG, "Telemetry packet sent: %" PRIu32,
                        telemetry_sequence);
            } else {
                ESP_LOGE(TAG, "Telemetry transmission failed: %s",
                        esp_err_to_name(err));
            }

            last_telemetry = now;
        }

        espnow_message_t message;
        if (espnow_receive(&message, pdMS_TO_TICKS(100)) == pdPASS) {
            process_message(&message);
        }
    }
}
