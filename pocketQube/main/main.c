/* Symmetric two-board ESP-NOW demo. Flash the same image to both boards. */

#include <inttypes.h>

#include "command_parser.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define DISCOVERY_PERIOD_MS 5000
#define COMMAND_PERIOD_MS 2000
#define LED_PULSE_MS 80
#define LED_GPIO 48 /* ESP32-S3 dev boards with a WS2812 */
#define TELEMETRY_PERIOD_MS 250


static const char *TAG = "P2P_APP";
static led_strip_handle_t led_strip;
static command_state_t cmd_state;

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


static void fill_dummy_telemetry(telemetry_data_t *t, const command_state_t *cmd)
{
    t->op_mode = cmd->op_mode;
    t->cmd_count = cmd->count;
    snprintf(t->cmd_echo, sizeof(t->cmd_echo), "%s", cmd->echo);
    t->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

    t->battery_voltage = 7.4f;
    t->gps_lat = 30.123456;
    t->gps_long = 76.123456;
    t->imu_ax = 0.1f;  t->imu_ay = 0.2f;  t->imu_az = 9.81f;
    t->imu_gx = 0.01f; t->imu_gy = 0.02f; t->imu_gz = 0.03f;
}
static void process_message(const espnow_message_t *message)
{
    if (message->type != ESPNOW_MESSAGE_COMMAND) {
        return;
    }

    char reply[CMD_REPLY_SIZE];
    cmd_result_t rc = command_handle(message->text, &cmd_state, reply, sizeof(reply));
    ESP_LOGI(TAG, "Command '%s' -> '%s' (rc=%d)", message->text, reply, rc);

    esp_err_t err = espnow_send(ESPNOW_MESSAGE_RESPONSE, message->sequence, reply);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not queue command response: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    configure_led();
    command_state_init(&cmd_state);
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
            fill_dummy_telemetry(&telemetry, &cmd_state);
            esp_err_t err = espnow_send_telemetry(&telemetry, telemetry_sequence);


            if (err == ESP_OK) {
                telemetry_sequence++;
                ESP_LOGI(TAG, "Te lemetry packet sent: %" PRIu32,
                        telemetry_sequence);
            } else {
                ESP_LOGE(TAG, "Telemetry transmission failed: %s",
                        esp_err_to_name(err));
            }

            last_telemetry = now;
        }

        if (cmd_state.pending_actions & ACTION_DEPLOY_BOOM) {
            // fire boom mechanism here
            cmd_state.pending_actions &= ~ACTION_DEPLOY_BOOM;
        }

        espnow_message_t message;
        if (espnow_receive(&message, pdMS_TO_TICKS(100)) == pdPASS) {
            process_message(&message);
        }
    }
}
