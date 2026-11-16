idf_component_register(
    SRCS "esp32-s3-can-2ch.c" "OD.c"
    INCLUDE_DIRS "."
    REQUIRES espressif__canopennode esp_driver_twai esp_timer
)
