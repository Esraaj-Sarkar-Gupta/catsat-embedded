idf_component_register(SRCS "espnow.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif nvs_flash
                    )