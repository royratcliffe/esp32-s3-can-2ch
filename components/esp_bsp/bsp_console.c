#include "bsp_console.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "bsp_can.h"
#include "bsp_can_config.h"
#include "bsp_wifi.h"
#include "bsp_xl2515.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CONSOLE_PROMPT "can2ch> "
#define CONSOLE_CONNECT_WAIT_MS 20000
#define CONSOLE_CONNECT_POLL_MS 250

static const char *TAG = "bsp_console";
static esp_console_repl_t *s_repl;

static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} s_set_args;

static struct {
    struct arg_lit *no_wait;
    struct arg_end *end;
} s_connect_args;

static struct {
    struct arg_str *channel;
    struct arg_int *kbps;
    struct arg_end *end;
} s_bitrate_args;

static const char *state_name(bsp_wifi_state_t state)
{
    switch (state) {
    case BSP_WIFI_STATE_CONNECTING:
        return "connecting";
    case BSP_WIFI_STATE_CONNECTED:
        return "connected";
    case BSP_WIFI_STATE_FAILED:
        return "failed";
    default:
        return "idle";
    }
}

/* SSIDs are raw bytes; replace control characters so they cannot disturb the terminal. */
static void print_ssid(const uint8_t *ssid, size_t max_len)
{
    size_t len = strnlen((const char *)ssid, max_len);
    if (len == 0) {
        fputs("<hidden>", stdout);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        putchar(ssid[i] < 0x20 || ssid[i] == 0x7F ? '?' : ssid[i]);
    }
}

static void print_status(const bsp_wifi_status_t *st)
{
    printf("SSID:     ");
    print_ssid((const uint8_t *)st->ssid, sizeof(st->ssid));
    printf(" (%s)\n", st->saved ? "saved" : "firmware default");
    printf("State:    %s\n", state_name(st->state));
    if (st->state == BSP_WIFI_STATE_CONNECTED) {
        printf("IP:       " IPSTR "\n", IP2STR(&st->ip));
        printf("Netmask:  " IPSTR "\n", IP2STR(&st->netmask));
        printf("Gateway:  " IPSTR "\n", IP2STR(&st->gateway));
        printf("RSSI:     %d dBm\n", st->rssi);
        printf("Channel:  %u\n", st->channel);
    }
}

static int wait_for_result(void)
{
    bsp_wifi_status_t st;
    printf("Waiting for connection...\n");
    for (int waited = 0; waited < CONSOLE_CONNECT_WAIT_MS; waited += CONSOLE_CONNECT_POLL_MS) {
        vTaskDelay(pdMS_TO_TICKS(CONSOLE_CONNECT_POLL_MS));
        if (bsp_wifi_get_status(&st) != ESP_OK) {
            return 1;
        }
        if (st.state == BSP_WIFI_STATE_CONNECTED) {
            print_status(&st);
            return 0;
        }
        if (st.state == BSP_WIFI_STATE_FAILED || st.state == BSP_WIFI_STATE_IDLE) {
            printf("Connection failed. Check the SSID/password with wifi_scan, then wifi_set or wifi_connect.\n");
            return 1;
        }
    }
    printf("Still connecting; check later with wifi_status.\n");
    return 0;
}

static int cmd_wifi_scan(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    bsp_wifi_status_t st;
    bsp_wifi_get_status(&st);

    printf("Scanning...\n");
    wifi_ap_record_t *aps = NULL;
    uint16_t count = 0;
    esp_err_t err = bsp_wifi_scan(&aps, &count);
    if (err != ESP_OK) {
        printf("Scan failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("%3s  %4s  %3s  %-11s  %-17s  %s\n", "#", "RSSI", "CH", "SECURITY", "BSSID", "SSID");
    for (uint16_t i = 0; i < count; i++) {
        const wifi_ap_record_t *ap = &aps[i];
        printf("%3u  %4d  %3u  %-11s  %02x:%02x:%02x:%02x:%02x:%02x  ",
               (unsigned)(i + 1), ap->rssi, ap->primary, bsp_wifi_auth_mode_name(ap->authmode),
               ap->bssid[0], ap->bssid[1], ap->bssid[2], ap->bssid[3], ap->bssid[4], ap->bssid[5]);
        print_ssid(ap->ssid, sizeof(ap->ssid));
        if (strncmp((const char *)ap->ssid, st.ssid, sizeof(ap->ssid)) == 0 && st.ssid[0] != '\0') {
            fputs("  *", stdout);
        }
        putchar('\n');
    }
    printf("%u network(s) found; * marks the configured SSID.\n", count);
    free(aps);
    return 0;
}

static int cmd_wifi_set(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&s_set_args) != 0) {
        arg_print_errors(stderr, s_set_args.end, argv[0]);
        return 1;
    }

    const char *ssid = s_set_args.ssid->sval[0];
    const char *password = s_set_args.password->count > 0 ? s_set_args.password->sval[0] : "";
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(password);
    if (ssid_len == 0 || ssid_len > BSP_WIFI_SSID_MAX_LEN) {
        printf("SSID must be 1-%d bytes.\n", BSP_WIFI_SSID_MAX_LEN);
        return 1;
    }
    if (pass_len != 0 && (pass_len < 8 || pass_len > BSP_WIFI_PASS_MAX_LEN)) {
        printf("Password must be empty (open network), 8-63 characters, or 64 hex digits.\n");
        return 1;
    }

    esp_err_t err = bsp_wifi_set_credentials(ssid, password);
    if (err != ESP_OK) {
        printf("Failed to apply credentials: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Credentials saved.\n");
    return wait_for_result();
}

static int cmd_wifi_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    bsp_wifi_status_t st;
    esp_err_t err = bsp_wifi_get_status(&st);
    if (err != ESP_OK) {
        printf("Status unavailable: %s\n", esp_err_to_name(err));
        return 1;
    }
    print_status(&st);
    return 0;
}

static int cmd_wifi_connect(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&s_connect_args) != 0) {
        arg_print_errors(stderr, s_connect_args.end, argv[0]);
        return 1;
    }

    esp_err_t err = bsp_wifi_reconnect();
    if (err != ESP_OK) {
        printf("Connect failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    return s_connect_args.no_wait->count > 0 ? 0 : wait_for_result();
}

static int cmd_wifi_forget(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_err_t err = bsp_wifi_forget_credentials();
    if (err != ESP_OK) {
        printf("Forget failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Saved credentials erased; using firmware defaults.\n");
    return wait_for_result();
}

static void print_bitrate(uint8_t channel, uint32_t current)
{
    uint32_t saved;
    bool has_saved = bsp_can_config_load_rate(channel, &saved);
    printf("CH%u (%s): %" PRIu32 " kbit/s", channel, channel == 1 ? "TWAI,   socketcand can0" : "XL2515, socketcand can1",
           current);
    if (has_saved) {
        printf(", saved %" PRIu32 " kbit/s\n", saved);
    } else {
        printf(", nothing saved (default %d kbit/s)\n", BSP_CAN_DEFAULT_KBPS);
    }
}

static int set_bitrate(uint8_t channel, uint32_t kbps)
{
    esp_err_t err = channel == 1 ? bsp_can1_set_rate(kbps)
                                 : (bsp_xl2515_set_rate(kbps) ? ESP_OK : ESP_FAIL);
    if (err != ESP_OK) {
        printf("CH%u: failed to set %" PRIu32 " kbit/s: %s\n", channel, kbps, esp_err_to_name(err));
        return 1;
    }
    err = bsp_can_config_save_rate(channel, kbps);
    if (err != ESP_OK) {
        printf("CH%u: set to %" PRIu32 " kbit/s but saving failed: %s\n", channel, kbps, esp_err_to_name(err));
        return 1;
    }
    printf("CH%u: set and saved %" PRIu32 " kbit/s\n", channel, kbps);
    return 0;
}

static int cmd_can_bitrate(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&s_bitrate_args) != 0) {
        arg_print_errors(stderr, s_bitrate_args.end, argv[0]);
        return 1;
    }

    if (s_bitrate_args.channel->count == 0) {
        print_bitrate(1, bsp_can1_get_rate());
        print_bitrate(2, bsp_xl2515_get_rate());
        return 0;
    }

    const char *ch = s_bitrate_args.channel->sval[0];
    bool ch1 = strcmp(ch, "1") == 0 || strcmp(ch, "all") == 0;
    bool ch2 = strcmp(ch, "2") == 0 || strcmp(ch, "all") == 0;
    if (!ch1 && !ch2) {
        printf("Channel must be 1, 2 or all.\n");
        return 1;
    }

    if (s_bitrate_args.kbps->count == 0) {
        if (ch1) {
            print_bitrate(1, bsp_can1_get_rate());
        }
        if (ch2) {
            print_bitrate(2, bsp_xl2515_get_rate());
        }
        return 0;
    }

    int kbps = s_bitrate_args.kbps->ival[0];
    if (kbps <= 0 || !bsp_can1_valid_rate((uint32_t)kbps)) {
        printf("Bit rate must be one of 25, 50, 100, 125, 250, 500, 800, 1000 (kbit/s).\n");
        return 1;
    }

    int ret = 0;
    if (ch1) {
        ret |= set_bitrate(1, (uint32_t)kbps);
    }
    if (ch2) {
        ret |= set_bitrate(2, (uint32_t)kbps);
    }
    return ret;
}

static int cmd_restart(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("Restarting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

static esp_err_t register_commands(void)
{
    s_set_args.ssid = arg_str1(NULL, NULL, "<ssid>", "network name; quote it if it contains spaces");
    s_set_args.password = arg_str0(NULL, NULL, "<password>", "passphrase; omit for an open network");
    s_set_args.end = arg_end(2);

    s_connect_args.no_wait = arg_lit0("n", "no-wait", "return immediately");
    s_connect_args.end = arg_end(1);

    s_bitrate_args.channel = arg_str0(NULL, NULL, "<1|2|all>", "CAN channel; omit to show both");
    s_bitrate_args.kbps = arg_int0(NULL, NULL, "<kbps>", "25, 50, 100, 125, 250, 500, 800 or 1000; omit to show");
    s_bitrate_args.end = arg_end(2);

    const esp_console_cmd_t cmds[] = {
        {
            .command = "wifi_scan",
            .help = "Scan for nearby Wi-Fi networks",
            .func = &cmd_wifi_scan,
        },
        {
            .command = "wifi_set",
            .help = "Save Wi-Fi credentials to flash and connect. "
                    "Example: wifi_set \"My Network\" \"my password\"",
            .func = &cmd_wifi_set,
            .argtable = &s_set_args,
        },
        {
            .command = "wifi_status",
            .help = "Show the configured SSID, connection state and IP address",
            .func = &cmd_wifi_status,
        },
        {
            .command = "wifi_connect",
            .help = "Retry connecting with the current credentials",
            .func = &cmd_wifi_connect,
            .argtable = &s_connect_args,
        },
        {
            .command = "wifi_forget",
            .help = "Erase saved credentials and revert to the firmware defaults",
            .func = &cmd_wifi_forget,
        },
        {
            .command = "can_bitrate",
            .help = "Show or set a CAN channel bit rate; a new rate is applied immediately and saved to flash. "
                    "Example: can_bitrate 1 500",
            .func = &cmd_can_bitrate,
            .argtable = &s_bitrate_args,
        },
        {
            .command = "restart",
            .help = "Reboot the device",
            .func = &cmd_restart,
        },
    };

    ESP_RETURN_ON_ERROR(esp_console_register_help_command(), TAG, "register help failed");
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cmds[i]), TAG, "register %s failed", cmds[i].command);
    }
    return ESP_OK;
}

esp_err_t bsp_console_start(void)
{
    if (s_repl) {
        return ESP_OK;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CONSOLE_PROMPT;
    repl_config.max_cmdline_length = 256;
    repl_config.task_stack_size = 6144;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t dev_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usb_serial_jtag(&dev_config, &repl_config, &s_repl),
                        TAG, "USB Serial/JTAG REPL init failed");
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t dev_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usb_cdc(&dev_config, &repl_config, &s_repl),
                        TAG, "USB CDC REPL init failed");
#elif CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t dev_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&dev_config, &repl_config, &s_repl),
                        TAG, "UART REPL init failed");
#else
    ESP_LOGW(TAG, "No console port configured; command line disabled");
    return ESP_OK;
#endif

    ESP_RETURN_ON_ERROR(register_commands(), TAG, "register commands failed");
    ESP_RETURN_ON_ERROR(esp_console_start_repl(s_repl), TAG, "start REPL failed");
    ESP_LOGI(TAG, "Command line ready; type 'help' for Wi-Fi commands");
    return ESP_OK;
}
