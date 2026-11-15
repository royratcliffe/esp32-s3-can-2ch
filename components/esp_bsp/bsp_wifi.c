#include "bsp_wifi.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_common.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define WIFI_NVS_NAMESPACE "ws_wifi"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASS "pass"
#define WIFI_SCAN_BUSY_RETRIES 10

static const char *TAG = "bsp_wifi";

static esp_netif_t *s_sta_netif;
static SemaphoreHandle_t s_lock;
static char s_ssid[BSP_WIFI_SSID_MAX_LEN + 1];
static bool s_saved;
static volatile bool s_auto_connect;
static volatile int s_retry_num;
static volatile bsp_wifi_state_t s_state = BSP_WIFI_STATE_IDLE;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_auto_connect) {
            s_state = BSP_WIFI_STATE_CONNECTING;
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = event_data;
        // Locally requested disconnects (reconfigure, scan) report ASSOC_LEAVE; the
        // requester has already decided whether to connect again.
        if (event->reason == WIFI_REASON_ASSOC_LEAVE) {
            return;
        }
        if (s_auto_connect && s_retry_num < WS_STA_MAXIMUM_RETRY) {
            s_retry_num++;
            s_state = BSP_WIFI_STATE_CONNECTING;
            ESP_LOGW(TAG, "Retrying Wi-Fi connection to \"%s\" (%d/%d, reason %d)",
                     s_ssid, s_retry_num, WS_STA_MAXIMUM_RETRY, event->reason);
            esp_wifi_connect();
        } else if (s_auto_connect) {
            s_auto_connect = false;
            s_state = BSP_WIFI_STATE_FAILED;
            ESP_LOGW(TAG, "Could not connect to \"%s\". Use the USB console: wifi_scan, wifi_set, wifi_connect.",
                     s_ssid);
        } else {
            s_state = BSP_WIFI_STATE_IDLE;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        s_retry_num = 0;
        s_state = BSP_WIFI_STATE_CONNECTED;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static bool password_is_valid(const char *password)
{
    size_t len = strlen(password);
    if (len == 0 || (len >= 8 && len <= 63)) {
        return true;
    }
    if (len != 64) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)password[i])) {
            return false;
        }
    }
    return true;
}

static esp_err_t validate_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t ssid_len = strlen(ssid);
    if (ssid_len == 0 || ssid_len > BSP_WIFI_SSID_MAX_LEN || !password_is_valid(password)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t build_config(wifi_config_t *cfg, const char *ssid, const char *password)
{
    ESP_RETURN_ON_ERROR(validate_credentials(ssid, password), TAG, "invalid credentials");

    memset(cfg, 0, sizeof(*cfg));
    memcpy(cfg->sta.ssid, ssid, strlen(ssid));
    memcpy(cfg->sta.password, password, strlen(password));
    cfg->sta.threshold.authmode = password[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    cfg->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    return ESP_OK;
}

static void load_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    nvs_handle_t nvs;
    s_saved = false;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        size_t sl = ssid_len;
        size_t pl = password_len;
        if (nvs_get_str(nvs, WIFI_NVS_KEY_SSID, ssid, &sl) == ESP_OK &&
            nvs_get_str(nvs, WIFI_NVS_KEY_PASS, password, &pl) == ESP_OK &&
            validate_credentials(ssid, password) == ESP_OK) {
            s_saved = true;
        }
        nvs_close(nvs);
    }
    if (!s_saved) {
        strlcpy(ssid, WS_STA_SSID, ssid_len);
        strlcpy(password, WS_STA_PASS, password_len);
    }
}

/* Caller holds s_lock (or is running before the console starts). */
static esp_err_t apply_and_connect(const char *ssid, const char *password)
{
    wifi_config_t cfg;
    ESP_RETURN_ON_ERROR(build_config(&cfg, ssid, password), TAG, "build config failed");

    s_auto_connect = false;
    esp_wifi_disconnect();
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "wifi config failed");
    strlcpy(s_ssid, ssid, sizeof(s_ssid));

    s_retry_num = 0;
    s_auto_connect = true;
    s_state = BSP_WIFI_STATE_CONNECTING;
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", s_ssid);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_auto_connect = false;
        s_state = BSP_WIFI_STATE_FAILED;
    }
    return err;
}

esp_err_t bsp_wifi_start(void)
{
    if (s_lock) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ws_nvs_init(), TAG, "nvs init failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init failed");
    // Credentials are persisted in our own NVS namespace.
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage failed");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL),
                        TAG, "register wifi event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL),
                        TAG, "register ip event failed");

    char ssid[BSP_WIFI_SSID_MAX_LEN + 1] = {0};
    char password[BSP_WIFI_PASS_MAX_LEN + 1] = {0};
    load_credentials(ssid, sizeof(ssid), password, sizeof(password));

    wifi_config_t cfg;
    ESP_RETURN_ON_ERROR(build_config(&cfg, ssid, password), TAG, "invalid Wi-Fi credentials");
    strlcpy(s_ssid, ssid, sizeof(s_ssid));
    memset(password, 0, sizeof(password));

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "wifi config failed");
    memset(&cfg, 0, sizeof(cfg));

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    s_retry_num = 0;
    s_auto_connect = true;
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s (%s credentials)", s_ssid, s_saved ? "saved" : "default");
    return esp_wifi_start();
}

esp_err_t bsp_wifi_set_credentials(const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_INVALID_STATE, TAG, "wifi not started");
    ESP_RETURN_ON_ERROR(validate_credentials(ssid, password), TAG, "invalid credentials");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, WIFI_NVS_KEY_SSID, ssid);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs, WIFI_NVS_KEY_PASS, password);
        }
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    if (err == ESP_OK) {
        s_saved = true;
        err = apply_and_connect(ssid, password);
    } else {
        ESP_LOGE(TAG, "saving credentials failed: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t bsp_wifi_forget_credentials(void)
{
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_INVALID_STATE, TAG, "wifi not started");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_erase_all(nvs);
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    if (err == ESP_OK) {
        s_saved = false;
        err = apply_and_connect(WS_STA_SSID, WS_STA_PASS);
    }
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t bsp_wifi_reconnect(void)
{
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_INVALID_STATE, TAG, "wifi not started");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_auto_connect = false;
    esp_wifi_disconnect();
    s_retry_num = 0;
    s_auto_connect = true;
    s_state = BSP_WIFI_STATE_CONNECTING;
    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", s_ssid);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_auto_connect = false;
        s_state = BSP_WIFI_STATE_FAILED;
    }
    xSemaphoreGive(s_lock);
    return err;
}

static int compare_rssi_desc(const void *a, const void *b)
{
    const wifi_ap_record_t *ra = a;
    const wifi_ap_record_t *rb = b;
    return (int)rb->rssi - (int)ra->rssi;
}

esp_err_t bsp_wifi_scan(wifi_ap_record_t **records, uint16_t *count)
{
    ESP_RETURN_ON_FALSE(records && count, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_INVALID_STATE, TAG, "wifi not started");
    *records = NULL;
    *count = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    // The driver rejects scans while a connection attempt is in progress.
    bool resume = false;
    if (s_state != BSP_WIFI_STATE_CONNECTED) {
        resume = s_auto_connect;
        s_auto_connect = false;
        esp_wifi_disconnect();
        s_state = BSP_WIFI_STATE_IDLE;
    }

    wifi_scan_config_t scan_cfg = {
        .show_hidden = true,
    };
    esp_err_t err = ESP_ERR_WIFI_STATE;
    for (int i = 0; i < WIFI_SCAN_BUSY_RETRIES && err == ESP_ERR_WIFI_STATE; i++) {
        err = esp_wifi_scan_start(&scan_cfg, true);
        if (err == ESP_ERR_WIFI_STATE) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    uint16_t n = 0;
    if (err == ESP_OK) {
        err = esp_wifi_scan_get_ap_num(&n);
    }
    if (err == ESP_OK && n > 0) {
        wifi_ap_record_t *list = calloc(n, sizeof(*list));
        if (!list) {
            esp_wifi_clear_ap_list();
            err = ESP_ERR_NO_MEM;
        } else {
            err = esp_wifi_scan_get_ap_records(&n, list);
            if (err == ESP_OK) {
                qsort(list, n, sizeof(*list), compare_rssi_desc);
                *records = list;
                *count = n;
            } else {
                free(list);
            }
        }
    } else if (err == ESP_OK) {
        esp_wifi_clear_ap_list();
    }

    if (resume) {
        s_retry_num = 0;
        s_auto_connect = true;
        s_state = BSP_WIFI_STATE_CONNECTING;
        if (esp_wifi_connect() != ESP_OK) {
            s_auto_connect = false;
            s_state = BSP_WIFI_STATE_FAILED;
        }
    }
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t bsp_wifi_get_status(bsp_wifi_status_t *status)
{
    ESP_RETURN_ON_FALSE(status, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_INVALID_STATE, TAG, "wifi not started");

    memset(status, 0, sizeof(*status));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(status->ssid, s_ssid, sizeof(status->ssid));
    status->saved = s_saved;
    status->state = s_state;
    xSemaphoreGive(s_lock);

    if (status->state == BSP_WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            status->rssi = ap.rssi;
            status->channel = ap.primary;
        }
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(s_sta_netif, &info) == ESP_OK) {
            status->ip = info.ip;
            status->netmask = info.netmask;
            status->gateway = info.gw;
        }
    }
    return ESP_OK;
}

const char *bsp_wifi_auth_mode_name(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2";
    case WIFI_AUTH_ENTERPRISE:
        return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:
        return "WAPI";
    case WIFI_AUTH_OWE:
        return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:
        return "WPA3-ENT192";
    default:
        return "OTHER";
    }
}
