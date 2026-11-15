#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "joystick_hid.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(joystick_hid_init());

    // Route the runtime console (logs and stdio) through the TinyUSB
    // CDC-ACM interface so it shares the USB cable with the HID joystick.
    const tinyusb_config_cdcacm_t acm_config = {
        .cdc_port = TINYUSB_CDC_ACM_0,
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_config));
    ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));

    const joystick_hid_report_t neutral_report = {0};
    while (true) {
        if (tud_mounted() && tud_hid_ready() &&
            !joystick_hid_send_report(&neutral_report)) {
            ESP_LOGW(TAG, "Failed to send joystick HID report");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
