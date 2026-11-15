idf_component_register(SRCS "esp32-s3-can-2ch.c" "ble_joystick.c" "OD.c" "canopen_node.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt nvs_flash esp_bsp esp_timer)
