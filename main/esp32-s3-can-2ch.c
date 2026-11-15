#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "joystick_hid.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(joystick_hid_init());

    const joystick_hid_report_t neutral_report = {0};
    while (true) {
        if (tud_mounted() && tud_hid_ready() &&
            !joystick_hid_send_report(&neutral_report)) {
            ESP_LOGW(TAG, "Failed to send joystick HID report");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
