#include <stdlib.h>
#include <string.h>

#include "CANopen.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"
#include "OD.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CANOPEN_TASK_PERIOD_MS 10
#define CANOPEN_FIRST_HEARTBEAT_MS 1000
#define CANOPEN_SDO_SERVER_TIMEOUT_MS 1000
#define CANOPEN_SDO_CLIENT_TIMEOUT_MS 500

static const char *TAG = "canopen";

static ODR_t write_mock_input(OD_stream_t *stream, const void *buf, OD_size_t count,
                              OD_size_t *count_written)
{
    if (stream == NULL || stream->object == NULL || stream->dataOrig == NULL
        || stream->dataLength > sizeof(int16_t)) {
        return ODR_DEV_INCOMPAT;
    }

    uint8_t previous[sizeof(int16_t)];
    memcpy(previous, stream->dataOrig, stream->dataLength);
    ODR_t result = OD_writeOriginal(stream, buf, count, count_written);
#ifdef CONFIG_CO_PDO
    if ((result == ODR_OK || result == ODR_PARTIAL)
        && memcmp(previous, stream->dataOrig, stream->dataLength) != 0) {
        CO_t *co = stream->object;
        const uint16_t input_index = stream->index == 0x2F01 ? 0x6000 : 0x6401;
        for (size_t i = 0; i < OD_CNT_TPDO; i++) {
            CO_TPDO_t *tpdo = &co->TPDO[i];
            const CO_PDO_common_t *pdo = &tpdo->PDO_common;
            for (uint8_t j = 0; j < pdo->mappedObjectsCount; j++) {
                if (pdo->OD_IO[j].stream.index == input_index
                    && pdo->OD_IO[j].stream.subIndex == stream->subIndex) {
                    CO_TPDOsendRequest(tpdo);
                    break;
                }
            }
        }
    }
#endif
    return result;
}

static void init_mock_inputs(CO_t *co)
{
    static OD_extension_t digital_extension;
    static OD_extension_t analog_extension;
    digital_extension = (OD_extension_t){ .object = co, .read = OD_readOriginal, .write = write_mock_input };
    analog_extension = (OD_extension_t){ .object = co, .read = OD_readOriginal, .write = write_mock_input };
    if (OD_extension_init(OD_ENTRY_H2F01, &digital_extension) != ODR_OK
        || OD_extension_init(OD_ENTRY_H2F02, &analog_extension) != ODR_OK) {
        ESP_LOGE(TAG, "Failed to initialise mock input controls");
        abort();
    }
}

static void check_canopen_result(CO_ReturnError_t result, const char *operation)
{
    if (result != CO_ERROR_NO) {
        ESP_LOGE(TAG, "%s failed (CANopen error %d)", operation, (int)result);
        abort();
    }
}

static const char *nmt_state_name(CO_NMT_internalState_t state)
{
    switch (state) {
    case CO_NMT_INITIALIZING:
        return "initializing";
    case CO_NMT_PRE_OPERATIONAL:
        return "pre-operational";
    case CO_NMT_OPERATIONAL:
        return "operational";
    case CO_NMT_STOPPED:
        return "stopped";
    default:
        return "unknown";
    }
}

void app_main(void)
{
    twai_node_handle_t twai_node;
    const twai_onchip_node_config_t twai_config = {
        .io_cfg.tx = CONFIG_CANOPEN_TX_GPIO,
        .io_cfg.rx = CONFIG_CANOPEN_RX_GPIO,
        .io_cfg.quanta_clk_out = GPIO_NUM_NC,
        .io_cfg.bus_off_indicator = GPIO_NUM_NC,
        .bit_timing.bitrate = CONFIG_CANOPEN_BITRATE_KBPS * 1000,
        .tx_queue_depth = 8,
    };

    ESP_ERROR_CHECK(twai_new_node_onchip(&twai_config, &twai_node));

    CO_t *co = CO_new(NULL, NULL);
    if (co == NULL) {
        ESP_LOGE(TAG, "Failed to allocate CANopen stack");
        abort();
    }

    ESP_LOGI(TAG, "Starting CO4017A0 mock: node ID %d, bitrate %d kbit/s, TX GPIO %d, RX GPIO %d",
             CONFIG_CANOPEN_NODE_ID, CONFIG_CANOPEN_BITRATE_KBPS,
             CONFIG_CANOPEN_TX_GPIO, CONFIG_CANOPEN_RX_GPIO);

    OD_RAM.digital_inputs[0] = 0xee;
    CO_NMT_reset_cmd_t reset = CO_RESET_COMM;
    while (reset != CO_RESET_APP) {
        uint32_t error_info = 0;
        CO_CANmodule_disable(co->CANmodule);
        check_canopen_result(CO_CANinit(co, twai_node, CONFIG_CANOPEN_BITRATE_KBPS),
                             "CAN driver initialization");
        check_canopen_result(CO_CANopenInit(co, NULL, NULL, OD, NULL, 0, CANOPEN_FIRST_HEARTBEAT_MS,
                                           CANOPEN_SDO_SERVER_TIMEOUT_MS, CANOPEN_SDO_CLIENT_TIMEOUT_MS,
                                           false, CONFIG_CANOPEN_NODE_ID,
                                           &error_info),
                             "CANopen initialization");
#ifdef CONFIG_CO_PDO
        check_canopen_result(CO_CANopenInitPDO(co, co->em, OD, CONFIG_CANOPEN_NODE_ID, &error_info),
                             "PDO initialization");
#endif
        init_mock_inputs(co);
        if (error_info != 0) {
            ESP_LOGW(TAG, "CANopen initialization reported warnings: 0x%08x", (unsigned)error_info);
        }
        CO_CANsetNormalMode(co->CANmodule);

        reset = CO_RESET_NOT;
        uint8_t mock_em_reported = 0;
        CO_NMT_internalState_t previous_nmt_state = CO_NMT_INITIALIZING;
        int64_t previous_time_us = esp_timer_get_time();
        while (reset == CO_RESET_NOT) {
            const int64_t current_time_us = esp_timer_get_time();
            const uint32_t elapsed_us = (uint32_t)(current_time_us - previous_time_us);

            reset = CO_process(co, false, elapsed_us, NULL);
#ifdef CONFIG_CO_PDO
            CO_process_RPDO(co, false, elapsed_us, NULL);
            CO_process_TPDO(co, false, elapsed_us, NULL);
#endif
            const uint8_t mock_em_requested = OD_RAM.mock_em_fault != 0;
            if (mock_em_requested != mock_em_reported) {
                if (mock_em_requested) {
                    CO_errorReport(co->em, CO_EM_GENERIC_ERROR, CO_EMC_GENERIC, 0x4D4F434B);
                    ESP_LOGW(TAG, "Simulated EMCY active");
                } else {
                    CO_errorReset(co->em, CO_EM_GENERIC_ERROR, 0);
                    ESP_LOGI(TAG, "Simulated EMCY cleared");
                }
                mock_em_reported = mock_em_requested;
            }

            const CO_NMT_internalState_t nmt_state = CO_NMT_getInternalState(co->NMT);
            if (nmt_state != previous_nmt_state) {
                ESP_LOGI(TAG, "NMT state: %s", nmt_state_name(nmt_state));
                previous_nmt_state = nmt_state;
            }

            previous_time_us = current_time_us;
            vTaskDelay(pdMS_TO_TICKS(CANOPEN_TASK_PERIOD_MS));
        }
    }

    ESP_LOGI(TAG, "NMT reset node: restarting");
    esp_restart();
}
