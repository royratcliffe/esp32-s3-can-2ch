set(SRC_FILES
    "bsp_can.c"
    "bsp_can_config.c"
    "bsp_common.c"
    "bsp_console.c"
    "bsp_rtc.c"
    "bsp_socketcand.c"
    "bsp_web.c"
    "bsp_wifi.c"
    "bsp_xl2515.c"
    "socketcand_protocol.c"
    "waveshare_twai_port.c")

idf_component_register(SRCS ${SRC_FILES}
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server esp_netif nvs_flash driver esp_timer
                    PRIV_REQUIRES esp_driver_gpio esp_driver_spi lwip console
                    EMBED_TXTFILES "root.html" "rtc.html")
