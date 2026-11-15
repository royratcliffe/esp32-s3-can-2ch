#ifndef BSP_WEB_H
#define BSP_WEB_H

#include "esp_err.h"

/**
 * @brief Start the HTTP server. Wi-Fi is started separately by bsp_wifi_start().
 *
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t bsp_web_init(void);

/**
 * @brief Start the scheduled CAN event task.
 *
 * @return ESP_OK if the task is running, or an ESP-IDF error code.
 */
esp_err_t bsp_web_start_event_task(void);

#endif
