#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi_types.h"

#define BSP_WIFI_SSID_MAX_LEN 32
#define BSP_WIFI_PASS_MAX_LEN 64

/**
 * @brief Station connection state.
 */
typedef enum {
    BSP_WIFI_STATE_IDLE = 0,
    BSP_WIFI_STATE_CONNECTING,
    BSP_WIFI_STATE_CONNECTED,
    BSP_WIFI_STATE_FAILED,
} bsp_wifi_state_t;

/**
 * @brief Snapshot of the station configuration and link state.
 */
typedef struct {
    char ssid[BSP_WIFI_SSID_MAX_LEN + 1];
    bool saved;
    bsp_wifi_state_t state;
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gateway;
    int8_t rssi;
    uint8_t channel;
} bsp_wifi_status_t;

/**
 * @brief Initialize NVS, netif and Wi-Fi, then start connecting in the background.
 *
 * Credentials saved with bsp_wifi_set_credentials() take precedence over the
 * compile-time WS_STA_SSID/WS_STA_PASS defaults. A failed connection does not
 * return an error, so the console remains available for reconfiguration.
 */
esp_err_t bsp_wifi_start(void);

/**
 * @brief Validate, save to NVS, and connect with new station credentials.
 *
 * @param ssid 1-32 byte network name.
 * @param password Empty for open networks, otherwise 8-63 characters or 64 hex digits.
 */
esp_err_t bsp_wifi_set_credentials(const char *ssid, const char *password);

/**
 * @brief Erase saved credentials and reconnect using the compile-time defaults.
 */
esp_err_t bsp_wifi_forget_credentials(void);

/**
 * @brief Restart connection attempts with the current credentials.
 */
esp_err_t bsp_wifi_reconnect(void);

/**
 * @brief Run a blocking scan for nearby access points.
 *
 * Pending connection attempts are stopped first when the station is not
 * connected, because the driver cannot scan while connecting.
 *
 * @param[out] records Heap-allocated array sorted by RSSI; release with free().
 * @param[out] count Number of entries in records.
 */
esp_err_t bsp_wifi_scan(wifi_ap_record_t **records, uint16_t *count);

/**
 * @brief Get the current station configuration and link state.
 */
esp_err_t bsp_wifi_get_status(bsp_wifi_status_t *status);

/**
 * @brief Human-readable name for a Wi-Fi authentication mode.
 */
const char *bsp_wifi_auth_mode_name(wifi_auth_mode_t mode);

#endif
