idf_component_register(SRCS "main.c" "espnow.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_netif nvs_flash esp_event esp_timer)
