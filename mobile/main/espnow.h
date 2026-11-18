#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

#define ESPNOW_CHANNEL 6
#define ESPNOW_TEXT_SIZE 32
#define ESPNOW_PROTOCOL_MAGIC UINT32_C(0x45534E57) /* "ESNW" */
#define ESPNOW_PROTOCOL_VERSION 1

typedef enum {
    ESPNOW_MESSAGE_DISCOVERY = 1,
    ESPNOW_MESSAGE_COMMAND = 2,
    ESPNOW_MESSAGE_RESPONSE = 3,
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

esp_err_t espnow_init(void);
esp_err_t espnow_send(espnow_message_type_t type, uint32_t sequence, const char *text);
bool espnow_peer_available(void);
BaseType_t espnow_receive(espnow_message_t *message, TickType_t timeout);
