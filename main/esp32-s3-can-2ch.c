#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "ble_joystick.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(ble_joystick_init());

    /* Demo input: sweep the X axis back and forth and toggle button 1 at
     * the extremes. Replace with real input, e.g. values received over
     * CAN. */
    int8_t x = 0;
    int8_t step = 4;
    for (;;) {
        if (ble_joystick_connected()) {
            uint8_t buttons = (x == 127 || x == -127) ? 0x01 : 0x00;
            ble_joystick_report(x, 0, buttons);
            if (x + step > 127 || x + step < -127) {
                step = -step;
            }
            x += step;
        }
        /* How often to update the joystick report is determined by the
         * delay below. Adjust as needed. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
