#include <stdio.h>
#include <string.h>

static const char *ESPNOW_TAG = "ESP_NOW";

// Define the broadcast MAC Address (FF:FF:FF:FF:FF:FF)
// ESP_NOW_ETH_ALEN = 6 -- The standard length of an ethernet/wifi MAC address
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

/*
 * This is the standard MAC address (where every bit is set to 1) for
 * the universal Layer 2 Broadcast Address -- packets are transmitted to
 * every single listening device on the same channel, rather than addressing
 * a single specific node.
 * 
 * We may have to change this in the future since we may be in the proximity
 * of other devices using the same protocol.
 */

typedef struct {
    uint8_t src_mac[ESP_NOW_ETH_ALEN]; // Store the MAC address
    uint8_t data[251]; // Store the message data
    // ESPNOW messages are capped at 250 characters

    int len; // Store the message length
} esp_now_event_t;

/* Define an RTOS Queue */
static QueueHandle_t espnow_queue;

/* --------- Callback Functions ---------- */

// Data RX Callback
void OnDataRecv(
    const esp_now_recv_info *recv_info, // A struct to encapsulate incoming packet data
    /*
    typedef struct esp_now_recv_info {
        uint8_t *src_addr;                      // 6-byte Source MAC address
        uint8_t *des_addr;                      // 6-byte Destination MAC address
        wifi_pkt_rx_ctrl_t *rx_ctrl;            // PHY layer metadata (RSSI, channel, rate)
    } esp_now_recv_info_t;
    */

   const uint8_t *data, // A read-only pointer to the raw incoming byte array in RAM
   /*
    * The buffer address passed into *data is owned by the network driver stack, and is only
    * valid for the duration of the callback execution. Once "this" function returns the WiFi
    * stack frees or reclaims this memory.
    * 
    * To process incoming data asynchrnously, the bytes in *data are copied to a queue or a
    * buffer before exiting the function.
    */

   int len, // The count of the valid data bytes inside data
) {
    const uint8_t *src_mac = recv_info->src_addr; // Extract the 6-digit MAC address

    // ESP Logging
    ESP_LOGI(
        ESPNOW_TAG,
        "Recieved %d bytes from %02X:%02X:%02X:%02X:%02X:%02X",
        len,
        src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]
    );

    ESP_LOGI( 
        ESPNOW_TAG,
        "Payload: %.*s", // A non-null-terminated string formatting
        len, data
    );

    /*
     * ESP-NOW frame payloads are transmitted over the air are raw-byte arrays, and
     * do not include the C-style trailing null terminator.
     */

    /* -- Copy the MAC address and payload -- */
    /*
     * An RTOS queue is used since it can be used without blocking
     * so the WiFi task exits instantly.
     * 
     */

    esp_now_event_t evt; // Instantiate the event
 
    memcpy(evt.src_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN); // Copy the MAC address into the buffer
    memcpy(evt.data, data, len); // Copy the data into the queue buffer
    evt.len = len; // And we need the length too

    // Push to the queue, with error handling
    if (
        xQueueSend(
            espnow_queue, // Queue datastructure
            &evt,   // Event reference
            0   // Timeout = 0
            ) != pdTRUE // If no error we move on
        ){
        ESP_LOGI(
            ESPNOW_TAG,
            "Queue full, data packet dropped!"
        );
    }

    return;
}

void espnow_process_task(void *pvParameters) // We consider a generic pointer
{
    /*
     * The task that unpacks the RTOS Queue to process RX
     *
     * What the remainder of this function does needs to be programmed. Fill
     * this space of the docstring with additional functionalities. We are
     * just Bones and All today.
     * 
     */
    esp_now_event_t evt;

    while(1) {
        // Wait until a packet arrives in the queue
        if(
            xQueueReceive(
                espnow_queue,
                &evt,
                portMAX_DELAY
            ) == pdTrue
        ) {
            // Assuming it's all going well

            char payload_str[251]; // An extra byte for the null-terminator
            memcpy(payload_str, evt.data, evt.len);
            payload_str[evt.len] = '\n'; // Throw in that null-terminator

            // Log the success
            ESP_LOGI(
                ESPNOW_TAG,
                "Process a packet from task! Payload: %s",
                payload_str
            );

            // The remainder of the logic goes here...
            // Write up the rest of the functions, won't you?

            // It feels so right to think of the time we could have tonight...
            // And it's so cold. It seems my hands are colder than yours; warm
            // me up inside.
        }
    }
}

// Data TX Callback
void OnDataSent(
    const esp_now_send_info_t *tx_info, // A read-only metadata structure
    /*
        typedef struct esp_now_send_info {
            uint8_t *des_addr;     // 6-byte Destination MAC address
        } esp_now_send_info_t;
    */
   esp_now_send_status_t status // Struct to store the status of TX
   /*
        typedef enum {
            ESP_NOW_SEND_SUCCESS = 0,   // Transmission successful
            ESP_NOW_SEND_FAIL    = 1,   // Transmission failed
        } esp_now_send_status_t;

        // We get a "success" if we get an ACK back from the other device.
   */
) {
    const uint8_t *mac_addr = tx_info->des_addr;
    ESP_LOGI(
        ESPNOW_TAG,
        "Transmission to %02X:%02X:%02X:%02X:%02X:%02X Status: %s",
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
        status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail"
    );ESP_NOW_SEND_FAIL == 1

    // TX failure handling
    if (status == ESP_NOW_SEND_FAIL) {
        
        // It's never over
        // All my blood for the sweetness of her laughter...
    }

    return;
}

/* ---------- Init Functions ---------- */
esp_err_t espnow_init(void) {
    /*
     * 1. Enable NVS -- Non-volatile storage.
     * 
     * The NVS stores small data items as key-value pairs in a deddicated
     * parition of the ESP32's flash memory. This data is retained across
     * reboots and power losses.
     * 
     * The WiFi PHY relies on the NVS to read hardware calibration parameters.
     * 
     */

        esp_err_t ret = nvs_flash_init();

        if (
            ret == ESP_ERR_NVS_NO_FREE_PAGES ||
            RET == ESP_ERR_NVS_NEW_VERSION_FOUND // This one really shouldn't happen in our framework
        ) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nsv_flash_init(); 
        }

        ESP_ERROR_CHECK(ret);

        // 2. Initialize Netif & System Event Loop
        /*
         * NETIF is the Network Interface Abstraction Layer
         *
         * This prepares the base netywork memory structures, and then
         * we create the system-wide event dispatch loop.
         */
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // 3. Configure and start Wi-Fi radio in Station (STA) mode
        /*
         * Allocate RAM for the WiFi task, and power the 2.4GHz Phy radio.
         * 
         * We force the WiFi config to be saved in RAM.
         * 
         */
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // Keep runtime configs in RAM to protect flash
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

        // Print local device MAC address for reference
        uint8_t local_mac[6];
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
        ESP_LOGI(ESPNOW_TAG, "Device STA MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
                local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);

        // 4. Allocate FreeRTOS Queue for asynchronous packet offloading
        espnow_queue = xQueueCreate(10, sizeof(esp_now_event_t)); // A thread-safe FIFO memory buffer
        
        // Memory allocated for it?
        if (espnow_queue == NULL) {
            ESP_LOGE(ESPNOW_TAG, "Failed to allocate memory for FreeRTOS Queue!");
            return ESP_ERR_NO_MEM;
        }

        // 5. Spawn background processing task (4096 bytes stack, Priority 5)
        BaseType_t task_ret = xTaskCreate(espnow_process_task, "espnow_process_task", 4096, NULL, 5, NULL);
        if (task_ret != pdPASS) {
            ESP_LOGE(ESPNOW_TAG, "Failed to create processing task!");
            return ESP_FAIL;
        }

        // 6. Initialize ESP-NOW protocol engine & attach callbacks
        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_register_recv_cb(OnDataRecv));
        ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent));

        // 7. Register Broadcast Peer in peer table
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
        peer_info.channel = 0;     // Match current channel (Channel 1)
        peer_info.encrypt = false; // Set true if using shared keys

        ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

        ESP_LOGI(ESPNOW_TAG, "ESP-NOW Subsystem successfully initialized!");
        return ESP_OK;
}
