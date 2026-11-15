idf_component_register(SRCS "esp32-s3-can-2ch.c" "ble_joystick.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt nvs_flash)
