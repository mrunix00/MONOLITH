/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <ipc.h>
#include <resource.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int _channel_command(rsrc_handle_t channel, uint64_t command_id, void *buffer, size_t size)
{
    return (int) rsmgr_control(channel, command_id, buffer, (uint32_t) size);
}

static int _connection_command(rsrc_handle_t channel, uint64_t command_id, connection_t *connection)
{
    if (connection == NULL)
        return -1;
    return _channel_command(channel, command_id, connection, sizeof(*connection));
}

rsrc_handle_t ipc_channel_create(const char *name)
{
    if (name != NULL) {
        size_t name_len = strlen(name);
        if (name_len == 0 || name_len >= IPC_CHANNEL_NAME_MAX)
            return -1;
    }

    return (rsrc_handle_t) syscall1(SYSCALL_IPC_CHANNEL_CREATE, (long) name);
}

rsrc_handle_t ipc_connect(const char *name)
{
    if (name == NULL)
        return -1;

    char path[IPC_CHANNEL_NAME_MAX + 10];
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= IPC_CHANNEL_NAME_MAX)
        return -1;

    memcpy(path, "channel:/", 9);
    memcpy(path + 9, name, name_len + 1);
    return rsmgr_open(path);
}

int ipc_await_connection(rsrc_handle_t channel, connection_t *connection)
{
    return _connection_command(channel, IPC_CHANNEL_COMMAND_WAIT_CONNECTION, connection);
}

int ipc_poll_connection(rsrc_handle_t channel, connection_t *connection)
{
    return _connection_command(channel, IPC_CHANNEL_COMMAND_POLL_CONNECTION, connection);
}

int ipc_accept_connection(rsrc_handle_t channel, connection_t *connection)
{
    return _connection_command(channel, IPC_CHANNEL_COMMAND_ACCEPT_CONNECTION, connection);
}

int ipc_reject_connection(rsrc_handle_t channel, connection_t *connection)
{
    return _connection_command(channel, IPC_CHANNEL_COMMAND_REJECT_CONNECTION, connection);
}

int ipc_send_to(rsrc_handle_t channel, connection_t *connection, void *data, size_t size)
{
    if (connection == NULL || data == NULL || size == 0)
        return -1;

    size_t request_size = sizeof(ipc_channel_send_message_in_t) + size;
    ipc_channel_send_message_in_t *request = malloc(request_size);
    if (request == NULL)
        return -1;

    request->connection_id = *connection;
    request->flags = 0;
    request->payload_len = size;
    memcpy(request->payload, data, size);
    int result
        = _channel_command(channel, IPC_CHANNEL_COMMAND_SEND_MESSAGE, request, request_size);
    free(request);
    return result;
}

int ipc_send(rsrc_handle_t channel, void *data, size_t size)
{
    if (data == NULL || size == 0)
        return -1;
    return rsmgr_write(channel, data, (uint32_t) size) < 0 ? -1 : 0;
}

int ipc_send_resource(rsrc_handle_t channel, connection_t *connection, rsrc_handle_t resource)
{
    if (connection == NULL || resource < 0)
        return -1;

    ipc_channel_send_object_in_t request = {
        .connection_id = *connection,
        .resource = resource,
        .rights_mask = 0,
        .flags = 0,
    };
    return _channel_command(channel, IPC_CHANNEL_COMMAND_SEND_OBJECT, &request, sizeof(request));
}

int ipc_receive_packet(rsrc_handle_t channel, void *buffer, size_t size)
{
    if (channel < 0 || buffer == NULL || size < sizeof(ipc_channel_packet_header_t))
        return -1;
    return _channel_command(channel, IPC_CHANNEL_COMMAND_RECV, buffer, size);
}

int ipc_receive(rsrc_handle_t channel, connection_t *sender, void *data, size_t size)
{
    if (channel < 0 || data == NULL || size == 0)
        return -1;

    size_t packet_size = sizeof(ipc_channel_recv_out_t) + size;
    ipc_channel_recv_out_t *packet = malloc(packet_size);
    if (packet == NULL)
        return -1;

    int result = ipc_receive_packet(channel, packet, packet_size);
    if (result <= 0) {
        free(packet);
        return result;
    }

    if (packet->header.type != IPC_CHANNEL_PACKET_MESSAGE || packet->header.payload_len > size) {
        free(packet);
        return -1;
    }

    if (sender != NULL)
        *sender = packet->header.sender_task_id;
    memcpy(data, packet->payload, packet->header.payload_len);
    result = (int) packet->header.payload_len;
    free(packet);
    return result;
}

int ipc_receive_resource(rsrc_handle_t channel, connection_t *sender, rsrc_handle_t *resource)
{
    unsigned char
        packet_buffer[sizeof(ipc_channel_recv_out_t) + sizeof(ipc_channel_object_payload_t)];
    int result = ipc_receive_packet(channel, packet_buffer, sizeof(packet_buffer));
    if (result <= 0)
        return result;

    ipc_channel_recv_out_t *packet = (ipc_channel_recv_out_t *) packet_buffer;
    if (packet->header.type != IPC_CHANNEL_PACKET_OBJECT
        || packet->header.payload_len < sizeof(ipc_channel_object_payload_t)) {
        return -1;
    }

    ipc_channel_object_payload_t *payload = (ipc_channel_object_payload_t *) packet->payload;
    if (sender != NULL)
        *sender = packet->header.sender_task_id;
    if (resource != NULL)
        *resource = (rsrc_handle_t) payload->resource;
    return 0;
}

int ipc_disconnect(rsrc_handle_t channel)
{
    int result = _channel_command(channel, IPC_CHANNEL_COMMAND_DISCONNECT, NULL, 0);
    rsmgr_close(channel);
    return result;
}
