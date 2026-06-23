/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <resource.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_CHANNEL_NAME_MAX 64

typedef struct
{
    uint64_t task_id;
} connection_t;

typedef rsrc_handle_t channel_id_t;

typedef enum {
    IPC_CHANNEL_COMMAND_SEND_MESSAGE = 1,
    IPC_CHANNEL_COMMAND_SEND_OBJECT = 2,
    IPC_CHANNEL_COMMAND_RECV = 3,
    IPC_CHANNEL_COMMAND_CREATE = 4,
    IPC_CHANNEL_COMMAND_POLL_CONNECTION = 5,
    IPC_CHANNEL_COMMAND_WAIT_CONNECTION = 6,
    IPC_CHANNEL_COMMAND_ACCEPT_CONNECTION = 7,
    IPC_CHANNEL_COMMAND_REJECT_CONNECTION = 8,
    IPC_CHANNEL_COMMAND_DISCONNECT = 9,
} ipc_channel_command_id_t;

typedef enum {
    IPC_CHANNEL_PACKET_MESSAGE = 1,
    IPC_CHANNEL_PACKET_OBJECT = 2,
    IPC_CHANNEL_PACKET_CONNECTION_REQUEST = 3,
    IPC_CHANNEL_PACKET_DISCONNECT = 4,
} ipc_channel_packet_type_t;

typedef struct
{
    uint32_t type;
    uint64_t connection_id;
    uint64_t sender_task_id;
    uint64_t payload_len;
    uint64_t flags;
} ipc_channel_packet_header_t;

typedef struct
{
    uint64_t connection_id;
    uint64_t flags;
    uint64_t payload_len;
    unsigned char payload[];
} ipc_channel_send_message_in_t;

typedef struct
{
    uint64_t connection_id;
    int64_t resource;
    uint64_t rights_mask;
    uint64_t flags;
} ipc_channel_send_object_in_t;

typedef struct
{
    int64_t resource;
    uint64_t resource_type;
    uint64_t rights;
} ipc_channel_object_payload_t;

typedef struct
{
    ipc_channel_packet_header_t header;
    unsigned char payload[];
} ipc_channel_recv_out_t;

channel_id_t ipc_new_channel(const char *name);
channel_id_t ipc_connect(const char *name);
int ipc_receive(channel_id_t channel_id, connection_t *sender, void *data, size_t size);
int ipc_receive_packet(channel_id_t channel_id, void *buffer, size_t size);
int ipc_receive_resource(channel_id_t channel_id, connection_t *sender, rsrc_handle_t *resource);
int ipc_await_connection(channel_id_t channel_id, connection_t *connection);
int ipc_poll_connection(channel_id_t channel_id, connection_t *connection);
int ipc_accept_connection(channel_id_t channel_id, connection_t *connection);
int ipc_reject_connection(channel_id_t channel_id, connection_t *connection);
int ipc_send_to(channel_id_t channel_id, connection_t *connection, void *data, size_t size);
int ipc_send(channel_id_t channel_id, void *data, size_t size);
int ipc_send_resource(channel_id_t channel_id, connection_t *connection, rsrc_handle_t resource);
int ipc_disconnect(channel_id_t channel_id);
