#include "bsp_can_config.h"

#include "bsp_can.h"
#include "esp_check.h"
#include "nvs.h"

#define CAN_NVS_NAMESPACE "ws_can"

static const char *TAG = "bsp_can_config";

static const char *rate_key(uint8_t channel)
{
    switch (channel) {
    case 1:
        return "ch1_kbps";
    case 2:
        return "ch2_kbps";
    default:
        return NULL;
    }
}

bool bsp_can_config_load_rate(uint8_t channel, uint32_t *kbps)
{
    *kbps = BSP_CAN_DEFAULT_KBPS;
    const char *key = rate_key(channel);
    if (!key) {
        return false;
    }

    nvs_handle_t nvs;
    if (nvs_open(CAN_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    uint32_t saved = 0;
    bool ok = nvs_get_u32(nvs, key, &saved) == ESP_OK && bsp_can1_valid_rate(saved);
    nvs_close(nvs);
    if (ok) {
        *kbps = saved;
    }
    return ok;
}

esp_err_t bsp_can_config_save_rate(uint8_t channel, uint32_t kbps)
{
    const char *key = rate_key(channel);
    ESP_RETURN_ON_FALSE(key && bsp_can1_valid_rate(kbps), ESP_ERR_INVALID_ARG, TAG, "invalid channel or rate");

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(CAN_NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs open failed");
    esp_err_t err = nvs_set_u32(nvs, key, kbps);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
