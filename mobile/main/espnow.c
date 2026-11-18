#include "espnow.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#define ESPNOW_RX_QUEUE_LENGTH 8

static const char *TAG = "ESPNOW";
static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    espnow_message_t message;
} receive_event_t;

static QueueHandle_t receive_queue;
static uint8_t local_mac[ESP_NOW_ETH_ALEN];
static uint8_t peer_mac[ESP_NOW_ETH_ALEN];
static bool peer_known;

static void log_mac(const char *label, const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X", label,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static esp_err_t add_peer(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

/* Wi-Fi callbacks copy data into a queue and return without parsing it. */
static void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (info == NULL || info->src_addr == NULL ||
        data == NULL || data_len != (int)sizeof(espnow_message_t)) {
        return;
    }

    receive_event_t event = {0};
    memcpy(event.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&event.message, data, sizeof(event.message));
    event.message.text[ESPNOW_TEXT_SIZE - 1] = '\0';
    if (xQueueSend(receive_queue, &event, 0) != pdPASS) {
        ESP_EARLY_LOGW(TAG, "Receive queue full; packet dropped");
    }
}

static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "TX %02X:%02X:%02X:%02X:%02X:%02X: %s",
             info->des_addr[0], info->des_addr[1], info->des_addr[2],
             info->des_addr[3], info->des_addr[4], info->des_addr[5],
             status == ESP_NOW_SEND_SUCCESS ? "delivered" : "not acknowledged");
}

esp_err_t espnow_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    log_mac("Local STA MAC", local_mac);

    receive_queue = xQueueCreate(ESPNOW_RX_QUEUE_LENGTH, sizeof(receive_event_t));
    if (receive_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(add_peer(broadcast_mac));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_received));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    return ESP_OK;
}

esp_err_t espnow_send(espnow_message_type_t type, uint32_t sequence, const char *text)
{
    if (text == NULL ||
        (type != ESPNOW_MESSAGE_DISCOVERY &&
         type != ESPNOW_MESSAGE_COMMAND &&
         type != ESPNOW_MESSAGE_RESPONSE)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *destination = broadcast_mac;
    if (type != ESPNOW_MESSAGE_DISCOVERY) {
        if (!peer_known) {
            return ESP_ERR_INVALID_STATE;
        }
        destination = peer_mac;
    }

    espnow_message_t message = {
        .magic = ESPNOW_PROTOCOL_MAGIC,
        .version = ESPNOW_PROTOCOL_VERSION,
        .type = (uint8_t)type,
        .sequence = sequence,
    };
    memcpy(message.sender_mac, local_mac, ESP_NOW_ETH_ALEN);
    snprintf(message.text, sizeof(message.text), "%s", text);
    return esp_now_send(destination, (const uint8_t *)&message, sizeof(message));
}

bool espnow_peer_available(void)
{
    return peer_known;
}

BaseType_t espnow_receive(espnow_message_t *message, TickType_t timeout)
{
    if (message == NULL || receive_queue == NULL) {
        return pdFALSE;
    }

    receive_event_t event;
    if (xQueueReceive(receive_queue, &event, timeout) != pdPASS) {
        return pdFALSE;
    }

    if (event.message.magic != ESPNOW_PROTOCOL_MAGIC ||
        event.message.version != ESPNOW_PROTOCOL_VERSION ||
        memcmp(event.source_mac, local_mac, ESP_NOW_ETH_ALEN) == 0 ||
        memcmp(event.source_mac, event.message.sender_mac, ESP_NOW_ETH_ALEN) != 0) {
        return pdFALSE;
    }

    if (!peer_known) {
        esp_err_t err = add_peer(event.source_mac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not add discovered peer: %s", esp_err_to_name(err));
            return pdFALSE;
        }
        memcpy(peer_mac, event.source_mac, ESP_NOW_ETH_ALEN);
        peer_known = true;
        log_mac("Using unicast peer", peer_mac);
    }

    const char *type_name = event.message.type == ESPNOW_MESSAGE_DISCOVERY ? "discovery" :
                            event.message.type == ESPNOW_MESSAGE_COMMAND ? "command" :
                            event.message.type == ESPNOW_MESSAGE_RESPONSE ? "response" : "unknown";
    ESP_LOGI(TAG, "RX %s #%" PRIu32 " from %02X:%02X:%02X:%02X:%02X:%02X: %s",
             type_name, event.message.sequence,
             event.source_mac[0], event.source_mac[1], event.source_mac[2],
             event.source_mac[3], event.source_mac[4], event.source_mac[5], event.message.text);

    *message = event.message;
    return pdPASS;
}
