#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

#define ESPNOW_CHANNEL 6
#define ESPNOW_TEXT_SIZE 64
#define ESPNOW_PROTOCOL_MAGIC UINT32_C(0x45534E57)
#define ESPNOW_PROTOCOL_VERSION 1
#define CMD_ECHO_SIZE 32
#define ESPNOW_SCIENCE_FIELDS 4

typedef enum {
    ESPNOW_MESSAGE_DISCOVERY = 1,
    ESPNOW_MESSAGE_COMMAND = 2,
    ESPNOW_MESSAGE_RESPONSE = 3,
    ESPNOW_MESSAGE_TELEMETRY = 4,
} espnow_message_type_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t sequence;
    uint8_t sender_mac[ESP_NOW_ETH_ALEN];
    char text[ESPNOW_TEXT_SIZE];
} espnow_message_t;

/* Keep this layout in sync with pocketQube/main/espnow.h. */
typedef struct __attribute__((packed)) {
    uint16_t team_id;
    uint8_t author;
    uint8_t op_mode;
    uint8_t type;
    uint32_t packet_token;
    uint32_t timestamp_ms;
    uint16_t cmd_count;
    char cmd_echo[CMD_ECHO_SIZE];
    float altitude_m;
    float temperature_c;
    uint32_t pressure_pa;
    float battery_voltage;
    float battery_current_ma;
    uint8_t gnss_hour, gnss_min, gnss_sec;
    float gnss_alt_m;
    double gps_lat, gps_long;
    uint8_t gnss_sats;
    float imu_gx, imu_gy, imu_gz;
    float imu_ax, imu_ay, imu_az;
    float mag_x, mag_y, mag_z;
    float solar_1_v, solar_2_v;
    uint8_t mech_state, stab_enabled;
    float stab_angle_deg, stab_error_deg;
    float science[ESPNOW_SCIENCE_FIELDS];
} telemetry_data_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t sequence;
    uint8_t sender_mac[ESP_NOW_ETH_ALEN];
    telemetry_data_t telemetry;
} espnow_telemetry_packet_t;

_Static_assert(sizeof(espnow_telemetry_packet_t) <= ESP_NOW_MAX_DATA_LEN,
               "Telemetry packet exceeds ESP-NOW payload limit");

esp_err_t espnow_init(void);
esp_err_t espnow_send_command(uint8_t author, uint32_t sequence, const char *text);
BaseType_t espnow_receive(uint8_t *packet, size_t capacity, size_t *length,
                          uint8_t source[ESP_NOW_ETH_ALEN], TickType_t timeout);
