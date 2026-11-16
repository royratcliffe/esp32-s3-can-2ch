set(app_sources "esp32-s3-can-2ch.c" "OD.c")
if(CONFIG_CANOPEN_RX_LOG)
    list(APPEND app_sources "can_rx_log.c")
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES espressif__canopennode esp_driver_twai esp_timer
)

if(CONFIG_CANOPEN_RX_LOG)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=twai_node_receive_from_isr")
endif()
