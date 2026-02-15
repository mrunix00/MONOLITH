/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_CHANNEL_NAME_MAX 64

typedef struct
{
    uint64_t task_id;
} connection_t;

typedef struct
{
    char name[IPC_CHANNEL_NAME_MAX];
    uint64_t owner_task_id;
} channel_t;

/*
 * Create a new IPC channel.
 * name: unique channel name.
 */
static inline int ipc_new_channel(const char *name)
{
    return syscall1(SYSCALL_IPC_NEW, (long) name);
}

/*
 * Connect to a named channel.
 * name: channel name to connect to.
 * channel: output channel handle populated on success.
 */
static inline int ipc_connect(const char *name, channel_t *channel)
{
    return syscall2(SYSCALL_IPC_REQUEST_CONNECTION, (long) name, (long) channel);
}

/*
 * Await a pending connection on a channel (owner side).
 * channel: channel handle to check.
 * connection: output pending connection (task_id set on success).
 */
static inline int ipc_await_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_WAIT_CONNECTION, (long) channel, (long) connection);
}

/*
 * Accept a pending connection (owner side).
 * channel: channel handle.
 * connection: connection to accept (task_id identifies client).
 */
static inline int ipc_accept_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_ACCEPT_CONNECTION, (long) channel, (long) connection);
}

/*
 * Reject a pending connection (owner side).
 * channel: channel handle.
 * connection: connection to reject (task_id identifies client).
 */
static inline int ipc_reject_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_REJECT_CONNECTION, (long) channel, (long) connection);
}

/*
 * Send data from the channel owner to a specific connection.
 * channel: channel handle.
 * connection: target connection/client.
 * data: message payload.
 * size: payload size in bytes.
 */
static inline int ipc_send_to(channel_t *channel, connection_t *connection, void *data, size_t size)
{
    return syscall4(
        SYSCALL_IPC_SEND_TO, (long) channel, (long) connection, (long) data, (long) size);
}

/*
 * Send data from a client to the owner, or broadcast from owner to all clients.
 * channel: channel handle.
 * data: message payload.
 * size: payload size in bytes.
 */
static inline int ipc_send(channel_t *channel, void *data, size_t size)
{
    return syscall3(SYSCALL_IPC_SEND, (long) channel, (long) data, (long) size);
}

/*
 * Receive data for the calling task (owner or client).
 * channel: channel handle.
 * sender: output sender info (task_id).
 * data: receive buffer.
 * size: buffer size in bytes.
 */
int ipc_receive(channel_t *channel, connection_t *sender, void *data, size_t size);

/*
 * Disconnect from a channel (client) or destroy it (owner).
 * channel: channel handle.
 */
static inline int ipc_disconnect(channel_t *channel)
{
    return syscall1(SYSCALL_IPC_DISCONNECT, (long) channel);
}
