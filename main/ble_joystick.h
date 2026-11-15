/*
 * BLE HID joystick (gamepad) peripheral over NimBLE.
 *
 * The device advertises as a HID-over-GATT gamepad with two signed 8-bit
 * axes (X, Y) and eight buttons. Hosts such as phones and PCs connect,
 * bond and receive input reports as notifications.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the NimBLE stack and start advertising as a BLE HID joystick.
 *
 * NVS must be initialised before calling this (bond keys and PHY
 * calibration data live in NVS).
 *
 * @return ESP_OK on success, otherwise an error from the NimBLE port.
 */
esp_err_t ble_joystick_init(void);

/**
 * Send a joystick input report to the connected host.
 *
 * @param x       X-axis position, -127 (left) to 127 (right).
 * @param y       Y-axis position, -127 (up) to 127 (down).
 * @param buttons Bit mask of buttons 1-8; bit 0 is button 1.
 *
 * @return ESP_OK if the notification was queued, ESP_ERR_INVALID_STATE if
 *         no host is connected and subscribed, ESP_FAIL otherwise.
 */
esp_err_t ble_joystick_report(int8_t x, int8_t y, uint8_t buttons);

/** @return true when a host is connected and subscribed to input reports. */
bool ble_joystick_connected(void);

#ifdef __cplusplus
}
#endif
