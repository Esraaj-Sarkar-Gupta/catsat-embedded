#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "ESP_NOW_DEMO";

// Broadcast MAC Address (FF:FF:FF:FF:FF:FF)
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ---------------- Callback Functions ---------------- */

// 1. Data Receive Callback
static void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    const uint8_t *src_mac = recv_info->src_addr;
    ESP_LOGI(TAG, "Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X",
             len, src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
    
    ESP_LOGI(TAG, "Payload: %.*s", len, data);
}

// 2. Data Sent Confirmation Callback (Updated signature)
static void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    const uint8_t *mac_addr = tx_info->des_addr;
    ESP_LOGI(TAG, "Transmission to %02X:%02X:%02X:%02X:%02X:%02X Status: %s",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
             status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

/* ---------------- Initialization ---------------- */

static void wifi_espnow_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t local_mac[6];
    // Use WIFI_IF_STA for ESP-IDF v5+ / v6.x
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    ESP_LOGI(TAG, "Local STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(OnDataRecv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent));

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

void app_main(void)
{
    wifi_espnow_init();

    uint32_t msg_counter = 0;
    char send_buffer[64];

    while (1) {
        snprintf(send_buffer, sizeof(send_buffer), "Ping Packet #%" PRIu32, msg_counter++);
        
        ESP_LOGI(TAG, "Sending: %s", send_buffer);

        esp_err_t result = esp_now_send(broadcast_mac, (uint8_t *)send_buffer, strlen(send_buffer));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error sending ESP-NOW packet: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}