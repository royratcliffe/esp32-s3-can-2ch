idf_component_register(SRCS "esp32-s3-can-2ch.c"
                    "joystick_hid.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_tinyusb)
