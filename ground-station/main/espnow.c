#include "espnow.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#define RX_QUEUE_LENGTH 12
#define RX_PACKET_MAX ESP_NOW_MAX_DATA_LEN

typedef struct {
    uint8_t source[ESP_NOW_ETH_ALEN];
    size_t length;
    uint8_t data[RX_PACKET_MAX];
} rx_event_t;

static const char *TAG = "GS_ESPNOW";
static QueueHandle_t rx_queue;
static uint8_t local_mac[ESP_NOW_ETH_ALEN];
static uint8_t peer_mac[3][ESP_NOW_ETH_ALEN];
static bool peer_known[3];
static uint32_t tx_sequence;

static esp_err_t add_peer(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (esp_now_is_peer_exist(mac)) return ESP_OK;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = ESPNOW_CHANNEL;
    return esp_now_add_peer(&peer);
}

static void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!info || !info->src_addr || !data || len <= 0 || len > RX_PACKET_MAX) return;
    rx_event_t event = {0};
    memcpy(event.source, info->src_addr, ESP_NOW_ETH_ALEN);
    event.length = (size_t)len;
    memcpy(event.data, data, event.length);
    if (xQueueSend(rx_queue, &event, 0) != pdPASS)
        ESP_EARLY_LOGW(TAG, "RX queue full; packet dropped");
}

static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "TX %02X:%02X:%02X:%02X:%02X:%02X %s",
             info->des_addr[0], info->des_addr[1], info->des_addr[2],
             info->des_addr[3], info->des_addr[4], info->des_addr[5],
             status == ESP_NOW_SEND_SUCCESS ? "sent" : "failed");
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
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(rx_event_t));
    if (!rx_queue) return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_received));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_LOGI(TAG, "Ground station ready; MAC %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);
    return ESP_OK;
}

/* Learn one unicast destination for author 1 (PocketQube) or 2 (CanSat)
 * from a valid telemetry packet. Commands remain addressed by author ID. */
static void learn_telemetry_peer(const rx_event_t *event)
{
    if (event->length == sizeof(espnow_message_t)) {
        const espnow_message_t *hello = (const espnow_message_t *)event->data;
        if (hello->magic == ESPNOW_PROTOCOL_MAGIC && hello->version == ESPNOW_PROTOCOL_VERSION &&
            hello->type == ESPNOW_MESSAGE_DISCOVERY &&
            memcmp(hello->sender_mac, event->source, ESP_NOW_ETH_ALEN) == 0 &&
            add_peer(event->source) == ESP_OK) {
            espnow_message_t response = {
                .magic = ESPNOW_PROTOCOL_MAGIC, .version = ESPNOW_PROTOCOL_VERSION,
                .type = ESPNOW_MESSAGE_DISCOVERY, .sequence = tx_sequence++,
            };
            memcpy(response.sender_mac, local_mac, ESP_NOW_ETH_ALEN);
            snprintf(response.text, sizeof(response.text), "HELLO");
            esp_now_send(event->source, (const uint8_t *)&response, sizeof(response));
        }
        return;
    }
    if (event->length != sizeof(espnow_telemetry_packet_t)) return;
    const espnow_telemetry_packet_t *p = (const espnow_telemetry_packet_t *)event->data;
    if (p->magic != ESPNOW_PROTOCOL_MAGIC || p->version != ESPNOW_PROTOCOL_VERSION ||
        p->type != ESPNOW_MESSAGE_TELEMETRY || p->telemetry.author < 1 || p->telemetry.author > 2 ||
        memcmp(p->sender_mac, event->source, ESP_NOW_ETH_ALEN) != 0) return;
    uint8_t author = p->telemetry.author;
    if (add_peer(event->source) != ESP_OK) return;
    memcpy(peer_mac[author], event->source, ESP_NOW_ETH_ALEN);
    peer_known[author] = true;
}

BaseType_t espnow_receive(uint8_t *packet, size_t capacity, size_t *length,
                          uint8_t source[ESP_NOW_ETH_ALEN], TickType_t timeout)
{
    if (!packet || !length || !source || !rx_queue) return pdFALSE;
    rx_event_t event;
    if (xQueueReceive(rx_queue, &event, timeout) != pdPASS) return pdFALSE;
    if (event.length > capacity) return pdFALSE;
    memcpy(packet, event.data, event.length);
    memcpy(source, event.source, ESP_NOW_ETH_ALEN);
    *length = event.length;
    learn_telemetry_peer(&event);
    return pdPASS;
}

esp_err_t espnow_send_command(uint8_t author, uint32_t sequence, const char *text)
{
    if (!text || (author != 1 && author != 2)) return ESP_ERR_INVALID_ARG;
    if (!peer_known[author]) return ESP_ERR_INVALID_STATE;
    espnow_message_t packet = {
        .magic = ESPNOW_PROTOCOL_MAGIC,
        .version = ESPNOW_PROTOCOL_VERSION,
        .type = ESPNOW_MESSAGE_COMMAND,
        .sequence = sequence,
    };
    memcpy(packet.sender_mac, local_mac, ESP_NOW_ETH_ALEN);
    if (snprintf(packet.text, sizeof(packet.text), "%s", text) >= sizeof(packet.text))
        return ESP_ERR_INVALID_SIZE;
    tx_sequence = sequence;
    return esp_now_send(peer_mac[author], (const uint8_t *)&packet, sizeof(packet));
}
