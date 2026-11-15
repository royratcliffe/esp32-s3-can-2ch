/*
 * SPDX-FileCopyrightText: 2026, Roy Ratcliffe, Northumberland, United Kingdom
 * SPDX-License-Identifier: MIT
 */

/*!
 * \file bsp_socketcand.h
 * \brief ESP32-S3 socketcand service and CAN receive forwarding interface.
 *
 * Declares service initialisation and task-context publication of received
 * frames from both physical CAN channels.
 */

#ifndef BSP_SOCKETCAND_H
#define BSP_SOCKETCAND_H

#include "bsp_common.h"

/*!
 * \brief Start the RAW/BCM TCP service and periodic UDP discovery after Wi-Fi initialisation.
 *
 * Call once from application startup after both CAN controllers are initialised.
 * Repeated calls after successful startup return ESP_OK without creating another task.
 *
 * \return ESP_OK if running, ESP_ERR_NO_MEM if queue allocation fails, or
 *         ESP_FAIL if socket setup or task creation fails. Failed startup releases
 *         the allocated service resources.
 */
esp_err_t bsp_socketcand_start(void);

/*!
 * \brief Timestamp and copy a received CAN data frame into the forwarding queue.
 *
 * The queue operation does not wait for space; overflow is logged and drops the
 * frame. Calls before service startup are ignored. Call from task context, not an ISR.
 *
 * \param channel Physical channel: 1 for can0, 2 for can1.
 * \param id CAN identifier without SocketCAN flag bits.
 * \param data Payload copied before return; may be NULL only when len is zero.
 * \param len Classic CAN payload length, from zero to eight bytes.
 * \param extended true for a 29-bit identifier, false for an 11-bit identifier.
 */
void bsp_socketcand_publish(uint8_t channel, uint32_t id, const uint8_t *data, uint8_t len, bool extended);

#endif
