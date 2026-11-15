#ifndef CANOPEN_NODE_H
#define CANOPEN_NODE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the CANopenNode stack on CAN channel 1 (on-chip TWAI).
 *
 * Creates the TWAI node on the Waveshare CAN1 pins and runs the CANopen
 * communication loop in a dedicated task. CAN channel 1 is owned by
 * CANopenNode after this call; do not initialise it through bsp_can.
 *
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t canopen_node_start(void);

#ifdef __cplusplus
}
#endif

#endif
