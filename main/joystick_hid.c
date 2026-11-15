#include "joystick_hid.h"

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

#include "class/hid/hid_device.h"

_Static_assert(sizeof(joystick_hid_report_t) == 10, "Joystick HID report size mismatch");

static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x04,       // Usage (Joystick)
    0xa1, 0x01,       // Collection (Application)
    0x05, 0x09,       //   Usage Page (Button)
    0x19, 0x01,       //   Usage Minimum (Button 1)
    0x29, 0x10,       //   Usage Maximum (Button 16)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x10,       //   Report Count (16)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0x05, 0x01,       //   Usage Page (Generic Desktop)
    0x09, 0x30,       //   Usage (X)
    0x09, 0x31,       //   Usage (Y)
    0x09, 0x32,       //   Usage (Z)
    0x09, 0x35,       //   Usage (Rz)
    0x16, 0x00, 0x80, //   Logical Minimum (-32768)
    0x26, 0xff, 0x7f, //   Logical Maximum (32767)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x04,       //   Report Count (4)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0xc0,             // End Collection
};

static const char *joystick_string_descriptor[] = {
    (const char[]){0x09, 0x04},
    "ESP32",
    "CAN Joystick",
    NULL,
    "Joystick",
};

#define JOYSTICK_CONFIG_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t joystick_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, JOYSTICK_CONFIG_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

esp_err_t joystick_hid_init(void)
{
    tinyusb_config_t tusb_config = TINYUSB_DEFAULT_CONFIG();
    tusb_config.descriptor.full_speed_config = joystick_configuration_descriptor;
    tusb_config.descriptor.string = joystick_string_descriptor;
    tusb_config.descriptor.string_count =
        sizeof(joystick_string_descriptor) / sizeof(joystick_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_config.descriptor.high_speed_config = joystick_configuration_descriptor;
#endif

    return tinyusb_driver_install(&tusb_config);
}

bool joystick_hid_send_report(const joystick_hid_report_t *report)
{
    return report != NULL && tud_hid_report(0, report, sizeof(*report));
}
