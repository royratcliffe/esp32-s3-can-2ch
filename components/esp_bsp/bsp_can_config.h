#ifndef BSP_CAN_CONFIG_H
#define BSP_CAN_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define BSP_CAN_DEFAULT_KBPS 250

/**
 * @brief Read the saved bit rate for a channel.
 *
 * @param channel 1 (on-chip TWAI) or 2 (XL2515).
 * @param[out] kbps Saved rate, or BSP_CAN_DEFAULT_KBPS when none is saved.
 * @return true if a valid saved rate was found.
 */
bool bsp_can_config_load_rate(uint8_t channel, uint32_t *kbps);

/**
 * @brief Save a channel bit rate to flash (NVS). Requires ws_nvs_init().
 *
 * @param channel 1 or 2.
 * @param kbps 25, 50, 100, 125, 250, 500, 800 or 1000.
 */
esp_err_t bsp_can_config_save_rate(uint8_t channel, uint32_t kbps);

#endif
