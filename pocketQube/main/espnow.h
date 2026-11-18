#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

#include "command_parser.h"

#define ESPNOW_CHANNEL 6
#define ESPNOW_TEXT_SIZE 64
#define ESPNOW_PROTOCOL_MAGIC UINT32_C(0x45534E57) /* "ESNW" */
#define ESPNOW_PROTOCOL_VERSION 1
#define ESPNOW_SCIENCE_FIELDS 4   /* adjust once the science experiment is decided */

typedef enum {
    ESPNOW_MESSAGE_DISCOVERY = 1,
    ESPNOW_MESSAGE_COMMAND = 2,
    ESPNOW_MESSAGE_RESPONSE = 3,
    ESPNOW_MESSAGE_TELEMETRY = 4,
} espnow_message_type_t;

/* Shared wire format: both boards must build with this same layout/version. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t sequence;
    uint8_t sender_mac[ESP_NOW_ETH_ALEN];
    char text[ESPNOW_TEXT_SIZE];
} espnow_message_t;

typedef struct __attribute__((packed)) {

    // ---- Metadata (team plan "debug" block) ----
    uint16_t team_id;            // GS builds ID as "<team_id>P"
    uint8_t  author;             // 1 = PocketQube, 2 = CanSat
    uint8_t  op_mode;            // 0 = flight, 1 = sim  -> GS writes 'F' / 'S'
    uint8_t  type;               // 0 = telemetry (team plan numbering)

    uint32_t packet_token;       // PACKET_COUNT (must survive resets, F1)
    uint32_t timestamp_ms;       // MISSION_TIME (must survive resets, F2)

    uint16_t cmd_count;          // COMMAND_COUNT
    char     cmd_echo[CMD_ECHO_SIZE];   // CMD_ECHO, 32 bytes, no commas

    // ---- Environment / power ----
    float    altitude_m;         // ALTITUDE, 0.1 m (from pressure, or SIMP in sim mode)
    float    temperature_c;      // TEMPERATURE, 0.1 C
    uint32_t pressure_pa;        // PRESSURE, 1 Pa
    float    battery_voltage;    // VOLTAGE, 0.1 V
    float    battery_current_ma; // CURRENT, 1 mA

    // ---- GNSS ----
    uint8_t  gnss_hour;          // GNSS_TIME (UTC hh:mm:ss)
    uint8_t  gnss_min;
    uint8_t  gnss_sec;
    float    gnss_alt_m;         // GNSS_ALTITUDE
    double   gps_lat;            // GNSS_LATITUDE  (double: float loses ~1 m)
    double   gps_long;           // GNSS_LONGITUDE
    uint8_t  gnss_sats;          // GNSS_SATS

    // ---- IMU ----
    float    imu_gx;             // ROT_RATE_X, deg/s, 0.1 resolution
    float    imu_gy;             // ROT_RATE_Y
    float    imu_gz;             // ROT_RATE_Z
    float    imu_ax;             // ACCEL_X, m/s^2, 0.001 resolution
    float    imu_ay;             // ACCEL_Y
    float    imu_az;             // ACCEL_Z

    // ---- Magnetometer (QMC5883L on boom) ----
    float    mag_x;              // MAG_X, milligauss, calibrated
    float    mag_y;              // MAG_Y
    float    mag_z;              // MAG_Z

    // ---- Solar / mechanisms ----
    float    solar_1_v;          // SOLAR_1, 0.01 V
    float    solar_2_v;          // SOLAR_2, 0.01 V
    uint8_t  mech_state;         // MECH_STATE bits: 0 SP1, 1 SP2, 2 boom, 3 camera rec

    // ---- Image stabilization (team-defined fields) ----
    uint8_t  stab_enabled;       // 1 = on (default)
    float    stab_angle_deg;     // current camera angle
    float    stab_error_deg;     // pointing error from north (-Y)

    // ---- Science experiment (placeholder) ----
    float    science[ESPNOW_SCIENCE_FIELDS];

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
esp_err_t espnow_send(espnow_message_type_t type, uint32_t sequence, const char *text);
bool espnow_peer_available(void);
BaseType_t espnow_receive(espnow_message_t *message, TickType_t timeout);
esp_err_t espnow_send_telemetry( telemetry_data_t const *telemetry, uint32_t sequence);
