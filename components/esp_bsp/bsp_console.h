#ifndef BSP_CONSOLE_H
#define BSP_CONSOLE_H

#include "esp_err.h"

/**
 * @brief Start the interactive command line on the primary console port.
 *
 * With CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG this is the ESP32-S3 native USB
 * port, usable from PuTTY or any serial terminal. Provides Wi-Fi scan and
 * credential configuration and CAN bit rate commands; type "help" for the list.
 * Call after bsp_wifi_start().
 */
esp_err_t bsp_console_start(void);

#endif
