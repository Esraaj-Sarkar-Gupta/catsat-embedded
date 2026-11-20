#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PC_UART UART_NUM_0
#define UART_BAUD 115200
#define UART_LINE_MAX 160
#define PI_F 3.14159265358979323846f

static uint32_t command_sequence;
static float previous_altitude;
static int64_t previous_altitude_us;

static void serial_line(const char *line)
{
    uart_write_bytes(PC_UART, line, strlen(line));
    uart_write_bytes(PC_UART, "\r\n", 2);
}

static void serial_raw(const char *kind, const char *text)
{
    char line[UART_LINE_MAX + 32];
    snprintf(line, sizeof(line), "%s%s", kind, text);
    serial_line(line);
}

static void emit_telemetry(const telemetry_data_t *t)
{
    char line[1200];
    float speed = 0.0f;
    int64_t now = esp_timer_get_time();
    if (previous_altitude_us && now > previous_altitude_us)
        speed = (t->altitude_m - previous_altitude) / ((now - previous_altitude_us) / 1000000.0f);
    previous_altitude = t->altitude_m;
    previous_altitude_us = now;
    float axg = t->imu_ax / 9.80665f, ayg = t->imu_ay / 9.80665f, azg = t->imu_az / 9.80665f;
    float atotal = sqrtf(axg * axg + ayg * ayg + azg * azg);
    float heading = atan2f(-t->mag_y, t->mag_x) * (180.0f / PI_F);
    if (heading < 0) heading += 360.0f;
    float roll = atan2f(t->imu_ay, t->imu_az) * (180.0f / PI_F);
    float pitch = atan2f(-t->imu_ax, sqrtf(t->imu_ay * t->imu_ay + t->imu_az * t->imu_az)) * (180.0f / PI_F);
    snprintf(line, sizeof(line),
        "Rel_alt_m:%.2f,GPS_rel_alt_m:%.2f,VSpeed_mps:%.2f,Pressure_hPa:%.2f,BMP_temp_C:%.2f,"
        "Battery_V:%.2f,Current_mA:%.1f,Ax_g:%.3f,Ay_g:%.3f,Az_g:%.3f,Atotal_g:%.3f,"
        "Gx_dps:%.2f,Gy_dps:%.2f,Gz_dps:%.2f,Roll_deg:%.1f,Pitch_deg:%.1f,Heading_deg:%.1f,"
        "Mx_uT:%.1f,My_uT:%.1f,Mz_uT:%.1f,Sats:%u,Lat:%.7f,Lon:%.7f,GPS_alt_m:%.2f,"
        "HDOP:0,Temperature_C:%.2f,Packet:%lu,Mode:%u,Cmd_count:%u,Author:%u",
        t->altitude_m, t->gnss_alt_m, speed, t->pressure_pa / 100.0f, t->temperature_c,
        t->battery_voltage, t->battery_current_ma, axg, ayg, azg, atotal,
        t->imu_gx, t->imu_gy, t->imu_gz, roll, pitch, heading, t->mag_x, t->mag_y, t->mag_z,
        (unsigned)t->gnss_sats, t->gps_lat, t->gps_long, t->gnss_alt_m,
        t->temperature_c, (unsigned long)t->packet_token, (unsigned)t->op_mode,
        (unsigned)t->cmd_count, (unsigned)t->author);
    serial_line(line);
}

static void process_packet(const uint8_t *data, size_t length)
{
    if (length == sizeof(espnow_telemetry_packet_t)) {
        const espnow_telemetry_packet_t *p = (const espnow_telemetry_packet_t *)data;
        if (p->magic != ESPNOW_PROTOCOL_MAGIC || p->version != ESPNOW_PROTOCOL_VERSION ||
            p->type != ESPNOW_MESSAGE_TELEMETRY) return;
        emit_telemetry(&p->telemetry);
        return;
    }
    if (length != sizeof(espnow_message_t)) return;
    const espnow_message_t *p = (const espnow_message_t *)data;
    if (p->magic != ESPNOW_PROTOCOL_MAGIC || p->version != ESPNOW_PROTOCOL_VERSION) return;
    if (p->type == ESPNOW_MESSAGE_RESPONSE) {
        const char *prefix = (strncmp(p->text, "ERR:", 4) == 0 ||
                              strncmp(p->text, "This command can only", 21) == 0)
                                 ? "ERROR: " : "REPLY: ";
        serial_raw(prefix, p->text);
    } else if (p->type == ESPNOW_MESSAGE_DISCOVERY) {
        serial_raw("INFO: ", "ESP-NOW peer discovered; waiting for telemetry identity");
    }
}

static void process_serial_command(char *line)
{
    /* PC framing: <author>,<command>\n, e.g. 1,ping or 2,mec deploy qube. */
    char *comma = strchr(line, ',');
    if (!comma) {
        serial_raw("ERROR: ", "expected command as <recipient>,<command>");
        return;
    }
    *comma++ = '\0';
    char *end = NULL;
    long author = strtol(line, &end, 10);
    while (*comma == ' ') comma++;
    if (!end || *end || (author != 1 && author != 2) || !*comma) {
        serial_raw("ERROR: ", "recipient must be 1 (PocketQube) or 2 (CanSat), followed by a command");
        return;
    }
    esp_err_t err = espnow_send_command((uint8_t)author, command_sequence++, comma);
    if (err != ESP_OK) {
        char msg[100];
        snprintf(msg, sizeof(msg), "command TX failed for recipient %ld: %s", author, esp_err_to_name(err));
        serial_raw("ERROR: ", msg);
    } else {
        char msg[UART_LINE_MAX];
        snprintf(msg, sizeof(msg), "%ld,%s", author, comma);
        serial_raw("TX: ", msg);
    }
}

static void serial_task(void *arg)
{
    (void)arg;
    char line[UART_LINE_MAX];
    size_t used = 0;
    uint8_t byte;
    while (true) {
        int n = uart_read_bytes(PC_UART, &byte, 1, pdMS_TO_TICKS(100));
        if (n <= 0) continue;
        if (byte == '\r') continue;
        if (byte == '\n') {
            line[used] = '\0';
            if (used) process_serial_command(line);
            used = 0;
        } else if (used + 1 < sizeof(line)) {
            line[used++] = (char)byte;
        } else {
            used = 0;
            serial_raw("ERROR: ", "serial command too long");
        }
    }
}

void app_main(void)
{
    const uart_config_t config = {
        .baud_rate = UART_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PC_UART, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PC_UART, &config));
    ESP_ERROR_CHECK(espnow_init());
    xTaskCreate(serial_task, "pc_serial", 3072, NULL, 5, NULL);
    serial_raw("INFO: ", "ground station ready at 115200 baud; waiting for telemetry");

    uint8_t packet[ESP_NOW_MAX_DATA_LEN];
    uint8_t source[ESP_NOW_ETH_ALEN];
    size_t length;
    while (true) {
        if (espnow_receive(packet, sizeof(packet), &length, source, pdMS_TO_TICKS(1000)) == pdPASS)
            process_packet(packet, length);
    }
}
