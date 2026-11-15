/*
 * SPDX-FileCopyrightText: 2026, Roy Ratcliffe, Northumberland, United Kingdom
 * SPDX-License-Identifier: MIT
 */

/*!
 * \file socketcand_protocol.h
 * \brief SocketCAN daemon protocol definitions for ESP32-S3 CAN adapter.
 * This header file defines the protocol structures, constants, and functions
 * used to implement the SocketCAN daemon over TCP for the ESP32-S3 dual CAN
 * adapter. It includes session state, incremental command parsing, frame
 * formatting, and beacon advertisement.
 */
#ifndef SOCKETCAND_PROTOCOL_H
#define SOCKETCAND_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SOCKETCAND_PORT 29536
#define SOCKETCAND_BEACON_PORT 42000
#define SOCKETCAND_COMMAND_SIZE 6400
#define SOCKETCAND_FRAME_SIZE 96
#define SOCKETCAND_BEACON_SIZE 320
#define SOCKETCAND_TX_JOBS 8
#define SOCKETCAND_RX_JOBS 8
#define SOCKETCAND_MUX_FRAMES 257

/*!
 * \brief Incremental command parser state retained between TCP reads.
 *
 * Initialise to zero. A completed command excludes its angle brackets and
 * remains valid until the next command is fed or tokenised.
 */
typedef struct {
    char command[SOCKETCAND_COMMAND_SIZE];
    size_t len;
    bool active;
} socketcand_stream_t;

/*!
 * \brief Classic CAN data frame exchanged between protocol and transport code.
 *
 * Channel 1 maps to can0; channel 2 maps to can1. The identifier excludes
 * SocketCAN flag bits, len is 0-8, and timestamp_us uses Unix microseconds.
 */
typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
    uint8_t channel;
    bool extended;
    int64_t timestamp_us;
} socketcand_frame_t;

/*! \brief One client-owned cyclic transmit job, keyed by identifier and format. */
typedef struct {
    bool used;
    socketcand_frame_t frame;
    int64_t interval_us;
    int64_t next_us;
} socketcand_tx_job_t;

/*! \brief Content mask and last relevant frame for one receive filter variant. */
typedef struct {
    uint8_t mask[8];
    bool seen;
    bool pending;
    socketcand_frame_t last;
} socketcand_rx_variant_t;

/*!
 * \brief Client-owned reception rule with optional multiplexed content masks.
 *
 * Variants are allocated on successful subscription and freed on replacement,
 * unsubscribe, mode change, or session reset.
 */
typedef struct {
    bool used;
    bool id_only;
    bool extended;
    bool flushing;
    uint32_t id;
    uint8_t mux_mask[8];
    size_t count;
    socketcand_rx_variant_t *variants;
    int64_t interval_us;
    int64_t next_us;
} socketcand_rx_job_t;

/*! \brief Per-client RAW/BCM state; initialise to zero and reset before disposal. */
typedef struct {
    uint8_t channel;
    bool raw;
    socketcand_tx_job_t tx[SOCKETCAND_TX_JOBS];
    socketcand_rx_job_t rx[SOCKETCAND_RX_JOBS];
} socketcand_session_t;

/*! \brief Free receive filters and clear all session state, including bus selection. */
void socketcand_session_reset(socketcand_session_t *session);

/*!
 * \brief Interpret a command and update the client's protocol state.
 *
 * \param session Client state, initially zeroed.
 * \param command Writable, null-terminated text without angle brackets;
 *                tokenisation modifies this buffer.
 * \param frame Receives the transmit frame only when NULL is returned.
 * \param now_us Nonnegative monotonic time in microseconds for BCM deadlines.
 * \return Static reply text (empty for successful BCM job commands), or NULL
 *         for an immediate send that the caller must submit to the controller.
 */
const char *socketcand_command(socketcand_session_t *session, char *command,
                              socketcand_frame_t *frame, int64_t now_us);

/*!
 * \brief Take one due cyclic transmission, advancing its monotonic deadline.
 *
 * Missed periods are skipped rather than replayed. Call until false to service
 * all due jobs. Returns false outside BCM mode.
 */
bool socketcand_next_transmit(socketcand_session_t *session, int64_t now_us, socketcand_frame_t *frame);

/*! \brief Apply BCM ID/content/multiplex filters to a received frame on the selected bus. */
void socketcand_receive(socketcand_session_t *session, const socketcand_frame_t *frame);

/*!
 * \brief Take one pending BCM notification whose throttle interval has elapsed.
 *
 * Call until false to drain due notifications, including retained changes when
 * no new CAN frames arrive. Each multiplex variant retains its latest relevant frame.
 */
bool socketcand_next_received(socketcand_session_t *session, int64_t now_us, socketcand_frame_t *frame);

/*!
 * \brief Consume one byte while preserving partial commands across TCP reads.
 *
 * \param stream Zero-initialised parser state retained between calls.
 * \param byte Next byte from the TCP stream.
 * \retval 1 A null-terminated command is available in stream->command.
 * \retval 0 Input accepted; no complete command is available yet.
 * \retval -1 Malformed or oversized input; discard the stream or disconnect.
 */
int socketcand_feed(socketcand_stream_t *stream, unsigned char byte);

/*!
 * \brief Encode a received frame with identifier width and microsecond timestamp.
 *
 * \param out Destination buffer for a null-terminated socketcand frame.
 * \param size Buffer capacity, including the null terminator.
 * \param frame Data frame with a valid identifier, length, and nonnegative timestamp.
 * \return Encoded length excluding the terminator, or zero on invalid input,
 *         formatting failure, or insufficient capacity. Discard output on failure.
 */
size_t socketcand_format_frame(char *out, size_t size, const socketcand_frame_t *frame);

/*! \brief Format a BCM notification using space-separated bytes, unlike RAW output. */
size_t socketcand_format_bcm_frame(char *out, size_t size, const socketcand_frame_t *frame);

/*!
 * \brief Encode a discovery beacon advertising both buses and the TCP endpoint.
 *
 * \param out Destination buffer for null-terminated CANBeacon XML.
 * \param size Buffer capacity, including the null terminator.
 * \param ip Trusted dotted-decimal IPv4 address; inserted without XML escaping.
 * \return Encoded length excluding the terminator, or zero on formatting failure
 *         or insufficient capacity. Discard output on failure.
 */
size_t socketcand_format_beacon(char *out, size_t size, const char *ip);

#endif
