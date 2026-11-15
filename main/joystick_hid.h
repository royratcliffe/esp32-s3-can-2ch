#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint16_t buttons;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rz;
} joystick_hid_report_t;

esp_err_t joystick_hid_init(void);
bool joystick_hid_send_report(const joystick_hid_report_t *report);
