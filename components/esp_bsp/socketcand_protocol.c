/*
 * SPDX-FileCopyrightText: 2026, Roy Ratcliffe, Northumberland, United Kingdom
 * SPDX-License-Identifier: MIT
 */

/*!
 * \file socketcand_protocol.c
 * \brief Platform-independent socketcand RAW and BCM protocol implementation.
 *
 * Implements incremental command parsing, session negotiation, classic CAN
 * frame validation and formatting, and UDP discovery beacon formatting.
 */

#include "socketcand_protocol.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*! \brief Frame angle-bracket commands without assuming TCP packet boundaries. */
int socketcand_feed(socketcand_stream_t *stream, unsigned char byte)
{
    if (byte == '<' && !stream->active) {
        stream->active = true;
        stream->len = 0;
        return 0;
    }
    if (byte == '>' && stream->active) {
        stream->command[stream->len] = '\0';
        stream->active = false;
        return 1;
    }
    if (stream->active && ((byte >= 0x20 && byte <= 0x7E && byte != '<') ||
                           byte == '\t' || byte == '\r' || byte == '\n')) {
        if (stream->len == sizeof(stream->command) - 1) {
            return -1;
        }
        stream->command[stream->len++] = (char)byte;
        return 0;
    }
    return !stream->active && isspace(byte) ? 0 : -1;
}

/*!
 * \brief Parse bounded, unsigned hexadecimal text without prefixes or signs.
 *
 * \param text Null-terminated token to parse.
 * \param max_digits Maximum accepted width, at most eight digits.
 * \param limit Largest accepted numeric value.
 * \param value Receives the parsed value on success; unchanged on failure.
 * \return true if the entire token satisfies the width and value limits.
 */
static bool parse_hex(const char *text, size_t max_digits, uint32_t limit, uint32_t *value)
{
    size_t len = strlen(text);
    if (!len || len > max_digits) {
        return false;
    }
    uint32_t result = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return false;
        }
        result = (result << 4) | digit;
    }
    if (result > limit) {
        return false;
    }
    *value = result;
    return true;
}

/*! \brief Return the next whitespace-delimited token, modifying the command buffer. */
static char *next_token(char **cursor)
{
    char *p = *cursor;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    char *start = p;
    while (*p && !isspace((unsigned char)*p)) {
        p++;
    }
    if (*p) {
        *p++ = '\0';
    }
    *cursor = p;
    return *start ? start : NULL;
}

/*! \brief Parse a decimal interval component with explicit overflow checking. */
static bool parse_decimal(const char *text, uint32_t limit, uint32_t *value)
{
    if (!text || !*text) {
        return false;
    }
    uint32_t result = 0;
    for (; *text; text++) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(*text - '0');
        if (result > limit / 10 || (result == limit / 10 && digit > limit % 10)) {
            return false;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return true;
}

/*! \brief Parse normalised seconds and microseconds without wall-clock dependencies. */
static bool parse_interval(char **cursor, int64_t *interval)
{
    uint32_t seconds, microseconds;
    if (!parse_decimal(next_token(cursor), UINT32_MAX, &seconds) ||
        !parse_decimal(next_token(cursor), 999999, &microseconds)) {
        return false;
    }
    *interval = (int64_t)seconds * 1000000 + microseconds;
    return true;
}

/*! \brief Parse a CAN identifier, preserving the eight-digit extended format. */
static bool parse_id(char **cursor, socketcand_frame_t *frame)
{
    char *id = next_token(cursor);
    if (!id || !parse_hex(id, 8, 0x1FFFFFFF, &frame->id)) {
        return false;
    }
    frame->extended = strlen(id) == 8;
    return frame->extended || frame->id <= 0x7FF;
}

/*! \brief Parse exactly one classic CAN payload with a decimal length from zero to eight. */
static bool parse_payload(char **cursor, socketcand_frame_t *frame)
{
    uint32_t len;
    if (!parse_decimal(next_token(cursor), 8, &len)) {
        return false;
    }
    frame->len = (uint8_t)len;
    for (size_t i = 0; i < len; i++) {
        char *token = next_token(cursor);
        uint32_t byte;
        if (!token || !parse_hex(token, 2, 0xFF, &byte)) {
            return false;
        }
        frame->data[i] = (uint8_t)byte;
    }
    return !next_token(cursor);
}

/*! \brief Release BCM resources while retaining the selected bus and current mode. */
static void clear_jobs(socketcand_session_t *session)
{
    for (size_t i = 0; i < SOCKETCAND_RX_JOBS; i++) {
        free(session->rx[i].variants);
    }
    memset(session->rx, 0, sizeof(session->rx));
    memset(session->tx, 0, sizeof(session->tx));
}

/*! \brief Free all dynamically allocated filters before clearing a client session. */
void socketcand_session_reset(socketcand_session_t *session)
{
    clear_jobs(session);
    memset(session, 0, sizeof(*session));
}

/*! \brief Compare CAN identifiers including the standard/extended distinction. */
static bool same_id(uint32_t id, bool extended, const socketcand_frame_t *frame)
{
    return id == frame->id && extended == frame->extended;
}

/*! \brief Install a validated ID-only, content, or multiplexed receive rule atomically. */
static const char *setup_receive(socketcand_session_t *session, char **cursor,
                                 const char *name, int64_t interval, socketcand_frame_t *frame)
{
    socketcand_rx_job_t *slot = NULL;
    for (size_t i = 0; i < SOCKETCAND_RX_JOBS; i++) {
        socketcand_rx_job_t *job = &session->rx[i];
        if (job->used && same_id(job->id, job->extended, frame)) {
            slot = job;
            break;
        }
        if (!job->used && !slot) {
            slot = job;
        }
    }
    if (!slot) {
        return "< error receive job limit >";
    }
    bool id_only = !strcmp(name, "subscribe");
    bool mux = !strcmp(name, "muxfilter");
    uint32_t count = 1;
    if (mux && (!parse_decimal(next_token(cursor), SOCKETCAND_MUX_FRAMES, &count) || count < 2)) {
        return "< error invalid multiplex count >";
    }
    socketcand_rx_job_t replacement = {
        .used = true, .id_only = id_only, .id = frame->id, .extended = frame->extended,
        .interval_us = interval, .count = mux ? count - 1 : 1,
    };
    replacement.variants = calloc(replacement.count, sizeof(*replacement.variants));
    if (!replacement.variants) {
        return "< error receive filter allocation failed >";
    }
    bool valid = true;
    if (mux) {
        for (size_t i = 0; i < count && valid; i++) {
            uint8_t *mask = i ? replacement.variants[i - 1].mask : replacement.mux_mask;
            for (size_t j = 0; j < 8; j++) {
                char *token = next_token(cursor);
                uint32_t byte;
                if (!token || !parse_hex(token, 2, 0xFF, &byte)) {
                    valid = false;
                    break;
                }
                mask[j] = (uint8_t)byte;
            }
        }
        valid = valid && !next_token(cursor);
    } else if (id_only) {
        valid = !next_token(cursor);
    } else {
        valid = parse_payload(cursor, frame);
        memcpy(replacement.variants[0].mask, frame->data, sizeof(frame->data));
    }
    if (!valid) {
        free(replacement.variants);
        return "< error invalid receive filter >";
    }
    free(slot->variants);
    *slot = replacement;
    return "";
}

/*! \brief Negotiate RAW/BCM mode and manage validated, client-owned CAN jobs. */
const char *socketcand_command(socketcand_session_t *session, char *command,
                              socketcand_frame_t *frame, int64_t now_us)
{
    char *cursor = command;
    char *name = next_token(&cursor);
    if (!name || now_us < 0) {
        return "< error syntax >";
    }
    if (!strcmp(name, "echo") && !next_token(&cursor)) {
        return "< echo >";
    }
    if (!strcmp(name, "open")) {
        char *bus = next_token(&cursor);
        if (!bus || next_token(&cursor)) {
            return "< error syntax >";
        }
        if (session->channel) {
            return "< error bus already open >";
        }
        if (strcmp(bus, "can0") && strcmp(bus, "can1")) {
            return "< error unknown bus >";
        }
        session->channel = !strcmp(bus, "can0") ? 1 : 2;
        return "< ok >";
    }
    if (!session->channel) {
        return "< error no bus selected >";
    }
    if (!strcmp(name, "rawmode") || !strcmp(name, "bcmmode")) {
        if (next_token(&cursor)) {
            return "< error syntax >";
        }
        bool raw = !strcmp(name, "rawmode");
        if (session->raw != raw) {
            clear_jobs(session);
            session->raw = raw;
        }
        return "< ok >";
    }
    *frame = (socketcand_frame_t){.channel = session->channel};
    if (!strcmp(name, "send")) {
        return parse_id(&cursor, frame) && parse_payload(&cursor, frame)
                   ? NULL : "< error invalid CAN frame >";
    }
    if (session->raw) {
        return "< error BCM command requires bcmmode >";
    }
    bool add = !strcmp(name, "add");
    bool receive = !strcmp(name, "subscribe") || !strcmp(name, "filter") || !strcmp(name, "muxfilter");
    bool update = !strcmp(name, "update");
    bool delete = !strcmp(name, "delete");
    bool unsubscribe = !strcmp(name, "unsubscribe");
    if (!add && !receive && !update && !delete && !unsubscribe) {
        return "< error unsupported command >";
    }
    int64_t interval = 0;
    if ((add || receive) && !parse_interval(&cursor, &interval)) {
        return "< error invalid interval >";
    }
    if (!parse_id(&cursor, frame)) {
        return "< error invalid CAN identifier >";
    }
    if (receive) {
        return setup_receive(session, &cursor, name, interval, frame);
    }
    if (unsubscribe) {
        if (next_token(&cursor)) {
            return "< error syntax >";
        }
        for (size_t i = 0; i < SOCKETCAND_RX_JOBS; i++) {
            socketcand_rx_job_t *job = &session->rx[i];
            if (job->used && same_id(job->id, job->extended, frame)) {
                free(job->variants);
                memset(job, 0, sizeof(*job));
                return "";
            }
        }
        return "< error receive job not found >";
    }
    if ((delete && next_token(&cursor)) || (!delete && !parse_payload(&cursor, frame))) {
        return "< error invalid CAN frame >";
    }
    socketcand_tx_job_t *slot = NULL;
    for (size_t i = 0; i < SOCKETCAND_TX_JOBS; i++) {
        socketcand_tx_job_t *job = &session->tx[i];
        if (job->used && same_id(job->frame.id, job->frame.extended, frame)) {
            slot = job;
            break;
        }
        if (!job->used && !slot && add) {
            slot = job;
        }
    }
    if (!slot) {
        return add ? "< error transmit job limit >" : "< error transmit job not found >";
    }
    if (delete) {
        memset(slot, 0, sizeof(*slot));
    } else if (add) {
        if (interval > INT64_MAX - now_us) {
            return "< error interval overflow >";
        }
        *slot = (socketcand_tx_job_t){
            .used = true, .frame = *frame, .interval_us = interval, .next_us = now_us + interval,
        };
        return NULL;
    } else {
        slot->frame = *frame;
    }
    return "";
}

/*! \brief Advance a periodic deadline without replaying missed timer expirations. */
static int64_t advance_deadline(int64_t deadline, int64_t interval, int64_t now_us)
{
    int64_t delay = interval - (now_us - deadline) % interval;
    return now_us > INT64_MAX - delay ? INT64_MAX : now_us + delay;
}

/*! \brief Extract the next due cyclic job using monotonic time. */
bool socketcand_next_transmit(socketcand_session_t *session, int64_t now_us, socketcand_frame_t *frame)
{
    if (!session->channel || session->raw || now_us < 0 || now_us == INT64_MAX) {
        return false;
    }
    socketcand_tx_job_t *due = NULL;
    for (size_t i = 0; i < SOCKETCAND_TX_JOBS; i++) {
        socketcand_tx_job_t *job = &session->tx[i];
        if (job->used && job->interval_us && now_us >= job->next_us &&
            (!due || job->next_us < due->next_us)) {
            due = job;
        }
    }
    if (!due) {
        return false;
    }
    *frame = due->frame;
    due->next_us = advance_deadline(due->next_us, due->interval_us, now_us);
    return true;
}

/*! \brief Retain relevant changes, comparing each multiplex value independently. */
void socketcand_receive(socketcand_session_t *session, const socketcand_frame_t *frame)
{
    if (!session->channel || session->raw || session->channel != frame->channel || frame->len > 8) {
        return;
    }
    for (size_t i = 0; i < SOCKETCAND_RX_JOBS; i++) {
        socketcand_rx_job_t *job = &session->rx[i];
        if (!job->used || !same_id(job->id, job->extended, frame)) {
            continue;
        }
        for (size_t n = 0; n < job->count; n++) {
            socketcand_rx_variant_t *variant = &job->variants[n];
            bool matches = true;
            bool changed = job->id_only || !variant->seen;
            for (size_t b = 0; b < 8; b++) {
                uint8_t data = b < frame->len ? frame->data[b] : 0;
                if ((data & job->mux_mask[b]) != (variant->mask[b] & job->mux_mask[b])) {
                    matches = false;
                    break;
                }
                if ((data & variant->mask[b]) != (variant->last.data[b] & variant->mask[b])) {
                    changed = true;
                }
            }
            if (!matches) {
                continue;
            }
            if (changed) {
                variant->last = *frame;
                memset(variant->last.data + frame->len, 0, 8 - frame->len);
                variant->seen = true;
                variant->pending = true;
            }
            break;
        }
    }
}

/*! \brief Flush retained BCM updates when their per-rule throttle window permits. */
bool socketcand_next_received(socketcand_session_t *session, int64_t now_us, socketcand_frame_t *frame)
{
    if (!session->channel || session->raw || now_us < 0) {
        return false;
    }
    for (size_t i = 0; i < SOCKETCAND_RX_JOBS; i++) {
        socketcand_rx_job_t *job = &session->rx[i];
        if (!job->used || (!job->flushing && now_us < job->next_us)) {
            continue;
        }
        for (size_t n = 0; n < job->count; n++) {
            socketcand_rx_variant_t *variant = &job->variants[n];
            if (variant->pending) {
                *frame = variant->last;
                variant->pending = false;
                job->flushing = true;
                return true;
            }
        }
        if (job->flushing) {
            job->next_us = now_us > INT64_MAX - job->interval_us ? INT64_MAX : now_us + job->interval_us;
            job->flushing = false;
        }
    }
    return false;
}

/*! \brief Share identifier and timestamp formatting between RAW and BCM notifications. */
static size_t format_frame(char *out, size_t size, const socketcand_frame_t *frame, bool bcm)
{
    if (frame->len > 8 || frame->timestamp_us < 0 ||
        frame->id > (frame->extended ? 0x1FFFFFFF : 0x7FF)) {
        return 0;
    }
    int n = snprintf(out, size, "< frame %0*" PRIX32 " %" PRId64 ".%06" PRId64 " ",
                     frame->extended ? 8 : 3, frame->id,
                     frame->timestamp_us / 1000000, frame->timestamp_us % 1000000);
    if (n < 0 || (size_t)n >= size) {
        return 0;
    }
    size_t pos = (size_t)n;
    for (uint8_t i = 0; i < frame->len; i++) {
        n = snprintf(out + pos, size - pos, bcm ? "%02X " : "%02X", frame->data[i]);
        if (n < 0 || (size_t)n >= size - pos) {
            return 0;
        }
        pos += (size_t)n;
    }
    n = snprintf(out + pos, size - pos, " >");
    return n >= 0 && (size_t)n < size - pos ? pos + (size_t)n : 0;
}

/*! \brief Encode a RAW notification with contiguous hexadecimal payload bytes. */
size_t socketcand_format_frame(char *out, size_t size, const socketcand_frame_t *frame)
{
    return format_frame(out, size, frame, false);
}

/*! \brief Encode a BCM notification with space-separated hexadecimal payload bytes. */
size_t socketcand_format_bcm_frame(char *out, size_t size, const socketcand_frame_t *frame)
{
    return format_frame(out, size, frame, true);
}

/*! \brief Build the adapter's XML advertisement using its current STA address. */
size_t socketcand_format_beacon(char *out, size_t size, const char *ip)
{
    int n = snprintf(out, size,
                     "<CANBeacon name=\"ESP32-S3-CAN-2CH\" type=\"adapter\" "
                     "description=\"Dual classic CAN Wi-Fi adapter\">"
                     "<URL>can://%s:%d</URL>"
                     "<Bus name=\"can0\"/><Bus name=\"can1\"/>"
                     "</CANBeacon>", ip, SOCKETCAND_PORT);
    return n >= 0 && (size_t)n < size ? (size_t)n : 0;
}
