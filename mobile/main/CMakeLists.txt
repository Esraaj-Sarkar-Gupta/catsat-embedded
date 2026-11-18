idf_component_register(SRCS "led.c" "main.c" "espnow.c" "command_parser.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi esp_netif nvs_flash esp_event led_strip)
