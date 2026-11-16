#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "can_rx_log.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define CAN_RX_LOG_QUEUE_SIZE 32
#define CAN_RX_LOG_BATCH_SIZE 8
#define CAN_RX_LOG_MAX_DATA 8

typedef struct {
    uint32_t id;
    uint16_t dlc;
    uint8_t length;
    bool extended;
    bool remote;
    uint8_t data[CAN_RX_LOG_MAX_DATA];
} can_rx_log_frame_t;

static const char *TAG = "can_rx";
static QueueHandle_t frames;
static twai_node_handle_t logged_node;
static portMUX_TYPE dropped_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t dropped;

esp_err_t __real_twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t *frame);

esp_err_t IRAM_ATTR __wrap_twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t *frame)
{
    const esp_err_t result = __real_twai_node_receive_from_isr(node, frame);
    if (result != ESP_OK || node != logged_node) {
        return result;
    }

    can_rx_log_frame_t record = {
        .id = frame->header.id,
        .dlc = frame->header.dlc,
        .extended = frame->header.ide,
        .remote = frame->header.rtr,
    };
    if (!record.remote && frame->buffer != NULL) {
        size_t length = record.dlc;
        if (length > frame->buffer_len) {
            length = frame->buffer_len;
        }
        if (length > CAN_RX_LOG_MAX_DATA) {
            length = CAN_RX_LOG_MAX_DATA;
        }
        record.length = length;
        for (size_t i = 0; i < length; i++) {
            record.data[i] = frame->buffer[i];
        }
    }

    if (xQueueSendFromISR(frames, &record, NULL) != pdTRUE) {
        portENTER_CRITICAL_ISR(&dropped_lock);
        dropped++;
        portEXIT_CRITICAL_ISR(&dropped_lock);
    }
    return result;
}

void can_rx_log_init(twai_node_handle_t node)
{
    frames = xQueueCreate(CAN_RX_LOG_QUEUE_SIZE, sizeof(can_rx_log_frame_t));
    if (frames == NULL) {
        ESP_LOGE(TAG, "Failed to allocate CAN receive log queue");
        abort();
    }
    logged_node = node;
}

void can_rx_log_process(void)
{
    portENTER_CRITICAL(&dropped_lock);
    const uint32_t lost = dropped;
    dropped = 0;
    portEXIT_CRITICAL(&dropped_lock);
    if (lost != 0) {
        ESP_LOGW(TAG, "Dropped %" PRIu32 " received frame log records", lost);
    }

    can_rx_log_frame_t frame;
    for (size_t i = 0; i < CAN_RX_LOG_BATCH_SIZE && xQueueReceive(frames, &frame, 0) == pdTRUE; i++) {
        char data[CAN_RX_LOG_MAX_DATA * 3 + 1] = "";
        for (size_t j = 0; j < frame.length; j++) {
            snprintf(data + j * 3, sizeof(data) - j * 3, "%02X ", (unsigned)frame.data[j]);
        }
        ESP_LOGI(TAG, "RX %s ID 0x%08" PRIX32 " DLC %u%s %s",
                 frame.extended ? "extended" : "standard", frame.id, (unsigned)frame.dlc,
                 frame.remote ? " RTR" : "", data);
    }
}
