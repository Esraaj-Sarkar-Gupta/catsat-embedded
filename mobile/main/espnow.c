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

// Define the broadcast MAC Address (FF:FF:FF:FF:FF:FF)
// ESP_NOW_ETH_ALEN = 6 -- The standard length of an ethernet/wifi MAC address
static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * This is the standard MAC address (where every bit is set to 1) for
 * the universal Layer 2 Broadcast Address -- packets are transmitted to
 * every single listening device on the same channel, rather than addressing
 * a single specific node.
 *
 * Discovery packets use this address. Once a peer is discovered, command and
 * response packets use that peer's unicast address instead.
 */

typedef struct {
    // Store the MAC address and complete protocol message together in the queue.
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    espnow_message_t message;
} receive_event_t;

/* Define an RTOS Queue */
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

/* --------- Callback Functions ---------- */

// Data RX Callback
/* Wi-Fi callbacks copy data into a queue and return without parsing it. */
static void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (info == NULL || info->src_addr == NULL ||
        data == NULL || data_len != (int)sizeof(espnow_message_t)) {
        return;
    }

    /*
     * The buffer address passed into data is owned by the network driver stack, and is only
     * valid for the duration of the callback execution. Once this function returns, the Wi-Fi
     * stack may reclaim this memory.
     *
     * To process incoming data asynchronously, the bytes are copied to a queue before exiting
     * the function.
     */
    receive_event_t event = {0};
    memcpy(event.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&event.message, data, sizeof(event.message));
    event.message.text[ESPNOW_TEXT_SIZE - 1] = '\0';
    /*
     * An RTOS queue is used without blocking so the Wi-Fi task exits instantly.
     */
    if (xQueueSend(receive_queue, &event, 0) != pdPASS) {
        ESP_EARLY_LOGW(TAG, "Receive queue full; packet dropped");
    }
}

// Data TX Callback
static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    // A successful transmission indicates that the receiver acknowledged the frame.
    ESP_LOGI(TAG, "TX %02X:%02X:%02X:%02X:%02X:%02X: %s",
             info->des_addr[0], info->des_addr[1], info->des_addr[2],
             info->des_addr[3], info->des_addr[4], info->des_addr[5],
             status == ESP_NOW_SEND_SUCCESS ? "delivered" : "not acknowledged");
}

esp_err_t espnow_init(void)
{
    /*
     * 1. Enable NVS -- Non-volatile storage.
     *
     * The NVS stores small data items as key-value pairs in a dedicated
     * partition of the ESP32's flash memory. This data is retained across
     * reboots and power losses.
     *
     * The Wi-Fi PHY relies on NVS to read hardware calibration parameters.
     */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. Initialize Netif & System Event Loop
    /*
     * NETIF is the Network Interface Abstraction Layer. This prepares the base
     * network memory structures, then creates the system-wide event dispatch loop.
     */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Configure and start Wi-Fi radio in Station (STA) mode
    /*
     * Allocate RAM for the Wi-Fi task and power the 2.4 GHz PHY radio. The
     * configuration is kept in RAM to avoid unnecessary writes to flash.
     */
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    log_mac("Local STA MAC", local_mac);

    // 4. Allocate a FreeRTOS queue for asynchronous packet offloading.
    receive_queue = xQueueCreate(ESPNOW_RX_QUEUE_LENGTH, sizeof(receive_event_t));
    if (receive_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 5. Initialize the ESP-NOW protocol engine and attach callbacks.
    ESP_ERROR_CHECK(esp_now_init());

    // 6. Register the broadcast peer in the peer table for discovery frames.
    ESP_ERROR_CHECK(add_peer(broadcast_mac));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_received));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    return ESP_OK;
}

esp_err_t espnow_send(espnow_message_type_t type, uint32_t sequence, const char *text)
{
    /*
     * Validate the caller's payload before constructing a frame. The message
     * type is restricted to the protocol's defined values so receivers do not
     * have to interpret arbitrary packet types.
     */
    if (text == NULL ||
        (type != ESPNOW_MESSAGE_DISCOVERY &&
         type != ESPNOW_MESSAGE_COMMAND &&
         type != ESPNOW_MESSAGE_RESPONSE)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *destination = broadcast_mac;
    /*
     * Discovery frames use the Layer 2 broadcast address so any listening
     * device on this channel can announce itself. Once a peer is discovered,
     * commands and responses are addressed to its specific unicast MAC.
     */
    if (type != ESPNOW_MESSAGE_DISCOVERY) {
        if (!peer_known) {
            return ESP_ERR_INVALID_STATE;
        }
        destination = peer_mac;
    }

    /*
     * ESP-NOW transmits raw bytes. Construct the complete fixed-size protocol
     * message locally, including the sender MAC and protocol version, before
     * handing it to the Wi-Fi driver.
     */
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
    /*
     * This function runs outside the Wi-Fi callback. It can wait for a queued
     * packet for up to timeout ticks, allowing packet validation and logging
     * without blocking the Wi-Fi task.
     */
    if (message == NULL || receive_queue == NULL) {
        return pdFALSE;
    }

    receive_event_t event;
    if (xQueueReceive(receive_queue, &event, timeout) != pdPASS) {
        return pdFALSE;
    }

    /*
     * Reject frames that do not belong to this protocol, packets received from
     * this device itself, and frames whose claimed sender MAC does not match
     * the link-layer source address.
     */
    if (event.message.magic != ESPNOW_PROTOCOL_MAGIC ||
        event.message.version != ESPNOW_PROTOCOL_VERSION ||
        memcmp(event.source_mac, local_mac, ESP_NOW_ETH_ALEN) == 0 ||
        memcmp(event.source_mac, event.message.sender_mac, ESP_NOW_ETH_ALEN) != 0) {
        return pdFALSE;
    }

    /*
     * The first valid sender becomes the unicast peer. Register it with the
     * ESP-NOW peer table before attempting unicast commands or responses.
     */
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
