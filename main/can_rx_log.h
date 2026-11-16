#pragma once

#include "esp_twai_types.h"

void can_rx_log_init(twai_node_handle_t node);
void can_rx_log_process(void);
