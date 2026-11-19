idf_component_register(SRCS "main.c" "espnow.c" "command_parser.c" ""drivers/i2c_bus.c""
                       INCLUDE_DIRS "." "drivers"
                       REQUIRES esp_wifi esp_netif nvs_flash esp_event led_strip esp_timer driver)
