#include "canopen_node.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "CANopen.h"
#include "OD.h"

#include "bsp_common.h"

static const char *TAG = "canopen";

enum {
    CANOPEN_NODE_ID = 1,
    CANOPEN_BITRATE_KBPS = 250,
    FIRST_HEARTBEAT_TIME_MS = 1000,
    SDO_SERVER_TIMEOUT_MS = 1000,
    SDO_CLIENT_TIMEOUT_MS = 500,
    CANOPEN_TASK_PERIOD_MS = 10,
    CANOPEN_TASK_STACK = 4096,
    CANOPEN_TASK_PRIORITY = 5,
};

static void canopen_task(void *arg)
{
    twai_node_handle_t node_hdl = arg;
    CO_t *co = CO_new(NULL, NULL);
    if (co == NULL) {
        ESP_LOGE(TAG, "CO_new failed");
        goto out;
    }

    CO_NMT_reset_cmd_t reset = CO_RESET_COMM;
    while (reset != CO_RESET_APP) {
        uint32_t err_info = 0;

        /* Standard CANopenNode communication reset sequence. */
        CO_CANmodule_disable(co->CANmodule);

        CO_ReturnError_t err = CO_CANinit(co, node_hdl, CANOPEN_BITRATE_KBPS);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG, "CO_CANinit failed: %d", err);
            break;
        }

        err = CO_CANopenInit(co, NULL, NULL, OD, NULL, 0, FIRST_HEARTBEAT_TIME_MS,
                             SDO_SERVER_TIMEOUT_MS, SDO_CLIENT_TIMEOUT_MS, false,
                             CANOPEN_NODE_ID, &err_info);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG, "CO_CANopenInit failed: %d (entry 0x%" PRIX32 ")", err, err_info);
            break;
        }

#ifdef CONFIG_CO_PDO
        err = CO_CANopenInitPDO(co, co->em, OD, CANOPEN_NODE_ID, &err_info);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG, "CO_CANopenInitPDO failed: %d (entry 0x%" PRIX32 ")", err, err_info);
            break;
        }
#endif

        CO_CANsetNormalMode(co->CANmodule);
        ESP_LOGI(TAG, "node %d on CAN1 (TX=%d RX=%d) at %d kbit/s",
                 CANOPEN_NODE_ID, WS_CAN1_TX_GPIO, WS_CAN1_RX_GPIO, CANOPEN_BITRATE_KBPS);

        reset = CO_RESET_NOT;
        int64_t last_us = esp_timer_get_time();
        while (reset == CO_RESET_NOT) {
            int64_t now_us = esp_timer_get_time();
            uint32_t time_diff_us = (uint32_t)(now_us - last_us);
            reset = CO_process(co, false, time_diff_us, NULL);
#ifdef CONFIG_CO_PDO
            CO_process_RPDO(co, false, time_diff_us, NULL);
            CO_process_TPDO(co, false, time_diff_us, NULL);
#endif
            last_us = now_us;
            vTaskDelay(pdMS_TO_TICKS(CANOPEN_TASK_PERIOD_MS));
        }
        ESP_LOGW(TAG, "NMT reset: %d", reset);
    }

    CO_delete(co);
out:
    twai_node_delete(node_hdl);
    vTaskDelete(NULL);
}

esp_err_t canopen_node_start(void)
{
    twai_node_handle_t node_hdl;
    const twai_onchip_node_config_t node_config = {
        .io_cfg.tx = WS_CAN1_TX_GPIO,
        .io_cfg.rx = WS_CAN1_RX_GPIO,
        .io_cfg.quanta_clk_out = GPIO_NUM_NC,
        .io_cfg.bus_off_indicator = GPIO_NUM_NC,
        .bit_timing.bitrate = CANOPEN_BITRATE_KBPS * 1000,
        .tx_queue_depth = 5,
    };
    ESP_RETURN_ON_ERROR(twai_new_node_onchip(&node_config, &node_hdl), TAG,
                        "twai_new_node_onchip failed");

    if (xTaskCreate(canopen_task, "canopen", CANOPEN_TASK_STACK, node_hdl,
                    CANOPEN_TASK_PRIORITY, NULL) != pdPASS) {
        twai_node_delete(node_hdl);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
