/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/rsmgr/rsmgr.h>
#include <kernel/tasking/task.h>

typedef uint64_t connection_t;

typedef enum {
    IPC_CHANNEL_COMMAND_SEND_MESSAGE = 1,
    IPC_CHANNEL_COMMAND_SEND_OBJECT = 2,
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
    rsrc_handle_t resource;
    uint64_t rights_mask;
    uint64_t flags;
} ipc_channel_send_object_in_t;

typedef struct
{
    rsrc_handle_t resource;
    uint64_t resource_type;
    uint64_t rights;
} ipc_channel_object_payload_t;

typedef struct
{
    ipc_channel_packet_header_t header;
    unsigned char payload[];
} ipc_channel_recv_out_t;

bool ipc_init();
rsrc_status_t ipc_channel_create(const char *name, rsrc_t **out_resource);
void ipc_task_cleanup(task_t *task);
