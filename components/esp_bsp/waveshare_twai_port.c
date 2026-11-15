#include "waveshare_twai_port.h"

#include <inttypes.h>

#include "bsp_can.h"
#include "bsp_can_config.h"
#include "bsp_common.h"
#include "bsp_console.h"
#include "bsp_rtc.h"
#include "bsp_socketcand.h"
#include "bsp_web.h"
#include "bsp_wifi.h"
#include "bsp_xl2515.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "waveshare_twai";

esp_err_t waveshare_main_wifi_sta_init(void)
{
    uint32_t can1_kbps;
    uint32_t can2_kbps;

    ESP_RETURN_ON_ERROR(ws_nvs_init(), TAG, "nvs init failed");
    ESP_RETURN_ON_ERROR(ws_logs_init(), TAG, "log init failed");
    ESP_RETURN_ON_ERROR(bsp_rtc_init(), TAG, "rtc init failed");
    bsp_can_config_load_rate(1, &can1_kbps);
    bsp_can_config_load_rate(2, &can2_kbps);
    ESP_LOGI(TAG, "CAN bit rates: CH1 %" PRIu32 " kbit/s, CH2 %" PRIu32 " kbit/s", can1_kbps, can2_kbps);
    ESP_RETURN_ON_ERROR(bsp_can1_init(can1_kbps), TAG, "can1 init failed");
    ESP_RETURN_ON_ERROR(bsp_xl2515_init(can2_kbps), TAG, "can2 init failed");
    ESP_RETURN_ON_ERROR(bsp_wifi_start(), TAG, "wifi start failed");
    ESP_RETURN_ON_ERROR(bsp_console_start(), TAG, "console start failed");
    ESP_RETURN_ON_ERROR(bsp_web_init(), TAG, "web init failed");
    ESP_RETURN_ON_ERROR(bsp_socketcand_start(), TAG, "socketcand start failed");

    ESP_RETURN_ON_ERROR(bsp_rtc_start_task(), TAG, "rtc task start failed");
    ESP_RETURN_ON_ERROR(bsp_can1_start_task(), TAG, "can1 task start failed");
    ESP_RETURN_ON_ERROR(bsp_xl2515_start_task(), TAG, "can2 task start failed");
    ESP_RETURN_ON_ERROR(bsp_web_start_event_task(), TAG, "event task start failed");
    return ESP_OK;
}

esp_err_t waveshare_main_wifi_ap_init(void)
{
    return waveshare_main_wifi_sta_init();
}

esp_err_t waveshare_twai_init(void)
{
    return waveshare_main_wifi_sta_init();
}

esp_err_t waveshare_twai_transmit(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ESP_OK;
}
