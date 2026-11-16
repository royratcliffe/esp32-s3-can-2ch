set(app_sources "esp32-s3-can-2ch.c" "OD.c" "ble_joystick.c")
if(CONFIG_CANOPEN_RX_LOG)
    list(APPEND app_sources "can_rx_log.c")
endif()

if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT CONFIG_BT_NIMBLE_ENABLED)
    message(FATAL_ERROR "The BLE joystick requires NimBLE. Enable Bluetooth and select NimBLE in menuconfig.")
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES espressif__canopennode esp_driver_twai esp_timer
    PRIV_REQUIRES bt nvs_flash
)

if(CONFIG_CANOPEN_RX_LOG)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=twai_node_receive_from_isr")
endif()
