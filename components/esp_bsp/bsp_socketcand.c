/*
 * SPDX-FileCopyrightText: 2026, Roy Ratcliffe, Northumberland, United Kingdom
 * SPDX-License-Identifier: MIT
 */

/*!
 * \file bsp_socketcand.c
 * \brief Dual-channel socketcand RAW/BCM TCP service with UDP discovery.
 *
 * Multiplexes client connections, forwards queued CAN frames, and advertises
 * both buses using the current Wi-Fi STA address.
 */

#include "bsp_socketcand.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "bsp_can.h"
#include "bsp_xl2515.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "socketcand_protocol.h"

#define CLIENT_COUNT 4
#define CLIENT_TX_SIZE 2048
#define RX_QUEUE_SIZE 64

/*!
 * \brief Service-task-owned connection state with bounded pending TCP output.
 *
 * A negative fd marks an unused slot. Partial commands and unsent bytes persist
 * across select iterations; CAN receive tasks never access this state.
 */
typedef struct {
    int fd;
    socketcand_session_t session;
    socketcand_stream_t stream;
    char tx[CLIENT_TX_SIZE];
    size_t tx_len;
} socketcand_client_t;

static const char *TAG = "socketcand";
static QueueHandle_t s_rx_queue;
static TaskHandle_t s_task;
static int s_listener = -1;
static int s_beacon = -1;
static socketcand_client_t s_clients[CLIENT_COUNT];

/*! \brief Close an active connection and reset its slot for reuse. */
static void close_client(socketcand_client_t *client)
{
    close(client->fd);
    socketcand_session_reset(&client->session);
    memset(client, 0, sizeof(*client));
    client->fd = -1;
}

/*!
 * \brief Copy protocol output into a client's bounded transmit buffer.
 *
 * \return true if queued, or false after disconnecting a client whose buffer
 *         cannot hold the complete message.
 */
static bool queue_text(socketcand_client_t *client, const char *text, size_t len)
{
    if (len > sizeof(client->tx) - client->tx_len) {
        ESP_LOGW(TAG, "Disconnecting slow client: output buffer full");
        close_client(client);
        return false;
    }
    memcpy(client->tx + client->tx_len, text, len);
    client->tx_len += len;
    return true;
}

/*!
 * \brief Attempt one non-blocking send while retaining any unsent suffix.
 *
 * Transient socket errors preserve pending output; terminal errors close the client.
 */
static void flush_client(socketcand_client_t *client)
{
    int sent = send(client->fd, client->tx, client->tx_len, 0);
    if (sent > 0) {
        client->tx_len -= (size_t)sent;
        memmove(client->tx, client->tx + sent, client->tx_len);
    } else if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        ESP_LOGW(TAG, "Client send failed: errno=%d", errno);
        close_client(client);
    }
}

/*! \brief Submit an immediate or cyclic frame through its selected physical controller. */
static bool transmit_frame(const socketcand_frame_t *frame)
{
    return frame->channel == 1
               ? bsp_can1_send(frame->id, frame->data, frame->len, frame->extended)
               : bsp_xl2515_send(frame->id, frame->data, frame->len, frame->extended);
}

/*!
 * \brief Dispatch a completed command to the protocol parser and selected CAN bus.
 *
 * \return true if the connection remains open, including reported command errors;
 *         false if queuing the reply disconnects the client.
 */
static bool process_command(socketcand_client_t *client)
{
    socketcand_frame_t frame;
    const char *reply = socketcand_command(&client->session, client->stream.command, &frame, esp_timer_get_time());
    if (!reply) {
        bool ok = transmit_frame(&frame);
        /* socketcand does not acknowledge successful send commands. */
        if (ok) {
            return true;
        }
        reply = "< error CAN transmit failed >";
    }
    if (!strncmp(reply, "< error", 7)) {
        ESP_LOGW(TAG, "%s", reply);
    }
    return !*reply || queue_text(client, reply, strlen(reply));
}

/*!
 * \brief Process one TCP read containing partial, complete, or coalesced commands.
 *
 * Malformed input, oversized commands, peer closure, and terminal receive errors
 * close the connection.
 */
static void read_client(socketcand_client_t *client)
{
    char input[256];
    int received = recv(client->fd, input, sizeof(input), 0);
    if (received <= 0) {
        if (!received || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            if (received < 0) {
                ESP_LOGW(TAG, "Client receive failed: errno=%d", errno);
            }
            close_client(client);
        }
        return;
    }
    for (int i = 0; i < received; i++) {
        int status = socketcand_feed(&client->stream, (unsigned char)input[i]);
        if (status < 0) {
            ESP_LOGW(TAG, "Disconnecting client: malformed or oversized command");
            close_client(client);
            return;
        }
        if (status == 1 && !process_command(client)) {
            return;
        }
    }
}

/*!
 * \brief Accept a non-blocking, low-latency client and queue the socketcand greeting.
 *
 * Connections are closed if setup fails or all client slots are occupied.
 */
static void accept_client(void)
{
    int fd = accept(s_listener, NULL, NULL);
    if (fd < 0) {
        ESP_LOGW(TAG, "Accept failed: errno=%d", errno);
        return;
    }
    int enabled = 1;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
        ESP_LOGW(TAG, "Client socket setup failed: errno=%d", errno);
        close(fd);
        return;
    }
    for (size_t i = 0; i < CLIENT_COUNT; i++) {
        if (s_clients[i].fd < 0) {
            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            s_clients[i].fd = fd;
            queue_text(&s_clients[i], "< hi >", strlen("< hi >"));
            return;
        }
    }
    ESP_LOGW(TAG, "Connection limit reached");
    close(fd);
}

/*!
 * \brief Drain due BCM notifications, including changes retained by throttled filters.
 */
static void forward_bcm(socketcand_client_t *client, int64_t now_us)
{
    socketcand_frame_t frame;
    while (client->fd >= 0 && socketcand_next_received(&client->session, now_us, &frame)) {
        char text[SOCKETCAND_FRAME_SIZE];
        size_t len = socketcand_format_bcm_frame(text, sizeof(text), &frame);
        if (!len) {
            ESP_LOGE(TAG, "Could not format BCM frame");
            continue;
        }
        queue_text(client, text, len);
    }
}

/*!
 * \brief Service one due cyclic send per client and delayed receive notifications.
 *
 * Limiting blocking controller calls keeps discovery and TCP processing responsive.
 * A failed cyclic send is reported without replaying missed timer periods.
 */
static void service_bcm(void)
{
    for (size_t i = 0; i < CLIENT_COUNT; i++) {
        socketcand_client_t *client = &s_clients[i];
        if (client->fd < 0) {
            continue;
        }
        int64_t now_us = esp_timer_get_time();
        socketcand_frame_t frame;
        if (socketcand_next_transmit(&client->session, now_us, &frame)) {
            if (!transmit_frame(&frame)) {
                const char *error = "< error cyclic CAN transmit failed >";
                ESP_LOGW(TAG, "%s", error);
                queue_text(client, error, strlen(error));
            }
        }
        forward_bcm(client, esp_timer_get_time());
    }
}

/*!
 * \brief Fan out a bounded batch of received frames to RAW clients and BCM filters.
 *
 * Each frame is removed from the shared queue once and copied to every subscribed
 * bus connection; slow clients are disconnected independently.
 */
static void forward_frames(void)
{
    socketcand_frame_t frame;
    /* Bound work so sustained CAN traffic cannot starve command handling. */
    for (size_t n = 0; n < RX_QUEUE_SIZE && xQueueReceive(s_rx_queue, &frame, 0) == pdTRUE; n++) {
        char text[SOCKETCAND_FRAME_SIZE];
        size_t len = socketcand_format_frame(text, sizeof(text), &frame);
        if (!len) {
            ESP_LOGE(TAG, "Could not format CAN frame");
            continue;
        }
        for (size_t i = 0; i < CLIENT_COUNT; i++) {
            socketcand_client_t *client = &s_clients[i];
            if (client->fd >= 0 && client->session.raw && client->session.channel == frame.channel) {
                queue_text(client, text, len);
            } else if (client->fd >= 0 && !client->session.raw) {
                socketcand_receive(&client->session, &frame);
                forward_bcm(client, esp_timer_get_time());
            }
        }
    }
}

/*!
 * \brief Broadcast discovery XML to the current STA subnet when IPv4 is available.
 *
 * Reading the address and netmask for every beacon accommodates DHCP changes
 * without restarting the service.
 */
static void send_beacon(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta || !esp_netif_is_netif_up(sta)) {
        return;
    }
    esp_netif_ip_info_t info;
    esp_err_t err = esp_netif_get_ip_info(sta, &info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot read STA address: %s", esp_err_to_name(err));
        return;
    }
    if (!info.ip.addr) {
        return;
    }
    char ip[16];
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
    char beacon[SOCKETCAND_BEACON_SIZE];
    size_t len = socketcand_format_beacon(beacon, sizeof(beacon), ip);
    if (!len) {
        ESP_LOGE(TAG, "Could not format discovery beacon");
        return;
    }
    struct sockaddr_in destination = {
        .sin_family = AF_INET,
        .sin_port = htons(SOCKETCAND_BEACON_PORT),
        .sin_addr.s_addr = info.ip.addr | ~info.netmask.addr,
    };
    if (sendto(s_beacon, beacon, len, 0, (struct sockaddr *)&destination, sizeof(destination)) != (int)len) {
        ESP_LOGW(TAG, "Beacon send failed: errno=%d", errno);
    }
}

/*!
 * \brief Multiplex TCP clients, CAN forwarding, and one-second discovery beacons.
 *
 * This task exclusively owns client sockets and their protocol state.
 * \param arg Unused FreeRTOS task argument.
 */
static void socketcand_task(void *arg)
{
    (void)arg;
    for (size_t i = 0; i < CLIENT_COUNT; i++) {
        s_clients[i].fd = -1;
    }
    int64_t next_beacon = 0;
    while (1) {
        int64_t now = esp_timer_get_time();
        if (now >= next_beacon) {
            send_beacon();
            next_beacon = now + 1000000;
        }
        forward_frames();
        service_bcm();
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(s_listener, &readfds);
        int maxfd = s_listener;
        for (size_t i = 0; i < CLIENT_COUNT; i++) {
            socketcand_client_t *client = &s_clients[i];
            if (client->fd >= 0) {
                FD_SET(client->fd, &readfds);
                if (client->tx_len) {
                    FD_SET(client->fd, &writefds);
                }
                if (client->fd > maxfd) {
                    maxfd = client->fd;
                }
            }
        }
        struct timeval timeout = {.tv_usec = 10000};
        int ready = select(maxfd + 1, &readfds, &writefds, NULL, &timeout);
        if (ready < 0) {
            if (errno != EINTR) {
                ESP_LOGE(TAG, "Socket select failed: errno=%d", errno);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }
        /* Accept last: the fd sets describe only the clients present before select. */
        for (size_t i = 0; i < CLIENT_COUNT; i++) {
            socketcand_client_t *client = &s_clients[i];
            if (client->fd >= 0 && FD_ISSET(client->fd, &writefds)) {
                flush_client(client);
            }
            if (client->fd >= 0 && FD_ISSET(client->fd, &readfds)) {
                read_client(client);
            }
        }
        if (FD_ISSET(s_listener, &readfds)) {
            accept_client();
        }
    }
}

/*! \brief Allocate service resources and launch the socketcand network task. */
esp_err_t bsp_socketcand_start(void)
{
    if (s_task) {
        return ESP_OK;
    }
    s_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(socketcand_frame_t));
    if (!s_rx_queue) {
        ESP_LOGE(TAG, "Cannot allocate CAN forwarding queue");
        return ESP_ERR_NO_MEM;
    }
    s_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    s_beacon = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int enabled = 1;
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(SOCKETCAND_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (s_listener < 0 || s_beacon < 0 ||
        setsockopt(s_listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0 ||
        setsockopt(s_beacon, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) < 0 ||
        fcntl(s_listener, F_SETFL, O_NONBLOCK) < 0 ||
        fcntl(s_beacon, F_SETFL, O_NONBLOCK) < 0 ||
        bind(s_listener, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(s_listener, CLIENT_COUNT) < 0) {
        ESP_LOGE(TAG, "Service socket setup failed: errno=%d", errno);
        goto failed;
    }
    if (xTaskCreate(socketcand_task, "socketcand", 4096, NULL, 3, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create service task");
        s_task = NULL;
        goto failed;
    }
    ESP_LOGI(TAG, "RAW/BCM TCP port %d, can0=CH1, can1=CH2; UDP beacon port %d",
             SOCKETCAND_PORT, SOCKETCAND_BEACON_PORT);
    return ESP_OK;

failed:
    if (s_listener >= 0) {
        close(s_listener);
        s_listener = -1;
    }
    if (s_beacon >= 0) {
        close(s_beacon);
        s_beacon = -1;
    }
    vQueueDelete(s_rx_queue);
    s_rx_queue = NULL;
    return ESP_FAIL;
}

/*! \brief Capture a system-clock timestamp and enqueue a copy of a received frame. */
void bsp_socketcand_publish(uint8_t channel, uint32_t id, const uint8_t *data, uint8_t len, bool extended)
{
    if (!s_rx_queue) {
        return;
    }
    if (channel < 1 || channel > 2 || len > 8 || (len && !data)) {
        ESP_LOGE(TAG, "Invalid received CAN frame");
        return;
    }
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) {
        ESP_LOGE(TAG, "Cannot timestamp CAN frame: errno=%d", errno);
        return;
    }
    socketcand_frame_t frame = {
        .id = id,
        .len = len,
        .channel = channel,
        .extended = extended,
        .timestamp_us = (int64_t)now.tv_sec * 1000000 + now.tv_usec,
    };
    if (len) {
        memcpy(frame.data, data, len);
    }
    if (xQueueSend(s_rx_queue, &frame, 0) != pdTRUE) {
        ESP_LOGW(TAG, "CAN forwarding queue full; dropping CH%u frame", channel);
    }
}
