/*
 * BLE HID joystick (gamepad) peripheral implemented with NimBLE.
 *
 * Implements the HID-over-GATT profile (HOGP): HID service with a gamepad
 * report map, plus the Battery and Device Information services that HOGP
 * hosts expect. Input reports carry two signed 8-bit axes and eight
 * buttons.
 */
#include "ble_joystick.h"

#include <string.h>

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_joystick";

#define DEVICE_NAME "ESP32-S3 Joystick"

/* GAP appearance: Human Interface Device - Gamepad. */
#define APPEARANCE_HID_GAMEPAD 0x03c4

/* 16-bit service UUIDs. */
#define UUID_SVC_HID 0x1812
#define UUID_SVC_BATTERY 0x180f
#define UUID_SVC_DEVICE_INFO 0x180a

/* 16-bit characteristic and descriptor UUIDs. */
#define UUID_CHR_HID_INFORMATION 0x2a4a
#define UUID_CHR_REPORT_MAP 0x2a4b
#define UUID_CHR_HID_CONTROL_POINT 0x2a4c
#define UUID_CHR_REPORT 0x2a4d
#define UUID_CHR_PROTOCOL_MODE 0x2a4e
#define UUID_CHR_BATTERY_LEVEL 0x2a19
#define UUID_CHR_MANUFACTURER_NAME 0x2a29
#define UUID_CHR_PNP_ID 0x2a50
#define UUID_DSC_REPORT_REFERENCE 0x2908

#define INPUT_REPORT_ID 1
#define INPUT_REPORT_LEN 3 /* X, Y, buttons */

/* Provided by the NimBLE host store; persists bonds when
 * CONFIG_BT_NIMBLE_NVS_PERSIST is enabled. */
void ble_store_config_init(void);

/* HID report map: gamepad with X/Y axes (-127..127) and 8 buttons. */
static const uint8_t report_map[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x05, /* Usage (Gamepad) */
    0xa1, 0x01, /* Collection (Application) */
    0x85, INPUT_REPORT_ID, /*   Report ID (1) */
    0x09, 0x01, /*   Usage (Pointer) */
    0xa1, 0x00, /*   Collection (Physical) */
    0x09, 0x30, /*     Usage (X) */
    0x09, 0x31, /*     Usage (Y) */
    0x15, 0x81, /*     Logical Minimum (-127) */
    0x25, 0x7f, /*     Logical Maximum (127) */
    0x75, 0x08, /*     Report Size (8) */
    0x95, 0x02, /*     Report Count (2) */
    0x81, 0x02, /*     Input (Data, Variable, Absolute) */
    0xc0,       /*   End Collection */
    0x05, 0x09, /*   Usage Page (Button) */
    0x19, 0x01, /*   Usage Minimum (Button 1) */
    0x29, 0x08, /*   Usage Maximum (Button 8) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x01, /*   Logical Maximum (1) */
    0x75, 0x01, /*   Report Size (1) */
    0x95, 0x08, /*   Report Count (8) */
    0x81, 0x02, /*   Input (Data, Variable, Absolute) */
    0xc0,       /* End Collection */
};

/* HID Information: bcdHID 1.11, no country code, normally connectable. */
static const uint8_t hid_information[] = {0x11, 0x01, 0x00, 0x02};

/* Report Reference descriptor: report ID 1, input report. */
static const uint8_t report_reference[] = {INPUT_REPORT_ID, 0x01};

/* PnP ID: vendor ID source USB, Espressif VID 0x303a, PID 1, version 1.0. */
static const uint8_t pnp_id[] = {0x02, 0x3a, 0x30, 0x01, 0x00, 0x00, 0x01};

static const char manufacturer_name[] = "Espressif";

static uint8_t battery_level = 100;
static uint8_t input_report[INPUT_REPORT_LEN];
static uint16_t input_report_handle;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool notify_enabled;
static uint8_t own_addr_type;

static void start_advertising(void);

static int chr_access(uint16_t conn, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid16_t *uuid16;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        uuid16 = BLE_UUID16(ctxt->chr->uuid);
        break;
    case BLE_GATT_ACCESS_OP_READ_DSC:
        uuid16 = BLE_UUID16(ctxt->dsc->uuid);
        break;
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* HID Control Point (suspend/exit suspend) and Protocol Mode
         * writes are accepted and ignored; only report mode is
         * supported. */
        return 0;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }

    const void *data;
    uint16_t len;

    switch (uuid16->value) {
    case UUID_CHR_HID_INFORMATION:
        data = hid_information;
        len = sizeof(hid_information);
        break;
    case UUID_CHR_REPORT_MAP:
        data = report_map;
        len = sizeof(report_map);
        break;
    case UUID_CHR_REPORT:
        data = input_report;
        len = sizeof(input_report);
        break;
    case UUID_DSC_REPORT_REFERENCE:
        data = report_reference;
        len = sizeof(report_reference);
        break;
    case UUID_CHR_PROTOCOL_MODE: {
        static const uint8_t report_mode = 0x01;
        data = &report_mode;
        len = sizeof(report_mode);
        break;
    }
    case UUID_CHR_BATTERY_LEVEL:
        data = &battery_level;
        len = sizeof(battery_level);
        break;
    case UUID_CHR_MANUFACTURER_NAME:
        data = manufacturer_name;
        len = sizeof(manufacturer_name) - 1;
        break;
    case UUID_CHR_PNP_ID:
        data = pnp_id;
        len = sizeof(pnp_id);
        break;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }

    return os_mbuf_append(ctxt->om, data, len) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_HID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_HID_INFORMATION),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_REPORT_MAP),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_HID_CONTROL_POINT),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP |
                             BLE_GATT_CHR_F_WRITE_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_PROTOCOL_MODE),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                             BLE_GATT_CHR_F_WRITE_NO_RSP |
                             BLE_GATT_CHR_F_WRITE_ENC,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_REPORT),
                    .access_cb = chr_access,
                    .val_handle = &input_report_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                             BLE_GATT_CHR_F_NOTIFY,
                    .descriptors =
                        (struct ble_gatt_dsc_def[]){
                            {
                                .uuid = BLE_UUID16_DECLARE(
                                    UUID_DSC_REPORT_REFERENCE),
                                .access_cb = chr_access,
                                .att_flags = BLE_ATT_F_READ |
                                             BLE_ATT_F_READ_ENC,
                            },
                            {0},
                        },
                },
                {0},
            },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_BATTERY),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_BATTERY_LEVEL),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {0},
            },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_DEVICE_INFO),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_MANUFACTURER_NAME),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(UUID_CHR_PNP_ID),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {0},
            },
    },
    {0},
};

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "connected; handle=%d", event->connect.conn_handle);
            conn_handle = event->connect.conn_handle;
        } else {
            ESP_LOGW(TAG, "connection failed; status=%d",
                     event->connect.status);
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        notify_enabled = false;
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == input_report_handle) {
            notify_enabled = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "input report notifications %s",
                     notify_enabled ? "enabled" : "disabled");
        }
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "encryption change; status=%d",
                 event->enc_change.status);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* Host lost its bond: delete ours and let it pair again. */
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) ==
            0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        return 0;
    }
}

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    struct ble_hs_adv_fields fields = {0};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = APPEARANCE_HID_GAMEPAD;
    fields.appearance_is_present = 1;
    fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(UUID_SVC_HID)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    fields.name = (const uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed; rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "advertising as \"%s\"", DEVICE_NAME);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no BLE address available; rc=%d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed; rc=%d", rc);
        return;
    }

    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "host reset; reason=%d", reason);
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_joystick_init(void)
{
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* HID hosts require bonding with an encrypted link; pair with
     * "just works" (no I/O capability). */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC |
                                 BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC |
                                   BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed; rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed; rc=%d", rc);
        return ESP_FAIL;
    }

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_device_appearance_set(APPEARANCE_HID_GAMEPAD);

    ble_store_config_init();

    nimble_port_freertos_init(host_task);

    return ESP_OK;
}

esp_err_t ble_joystick_report(int8_t x, int8_t y, uint8_t buttons)
{
    input_report[0] = (uint8_t)x;
    input_report[1] = (uint8_t)y;
    input_report[2] = buttons;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !notify_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om =
        ble_hs_mbuf_from_flat(input_report, sizeof(input_report));
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(conn_handle, input_report_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

bool ble_joystick_connected(void)
{
    return conn_handle != BLE_HS_CONN_HANDLE_NONE && notify_enabled;
}
