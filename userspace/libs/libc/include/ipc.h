/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include "unistd.h"
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint64_t task_id;
} connection_t;

typedef struct
{
    char *name;
    connection_t *connections;
    size_t connections_count;
    size_t max_connections;
} channel_t;

static inline int ipc_new(const char *name)
{
    return syscall1(SYSCALL_IPC_NEW, (long) name);
}

static inline int ipc_request_connection(const char *name, channel_t *channel)
{
    return syscall2(SYSCALL_IPC_REQUEST_CONNECTION, (long) name, (long) channel);
}

static inline int ipc_wait_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_WAIT_CONNECTION, (long) channel, (long) connection);
}

static inline int ipc_accept_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_ACCEPT_CONNECTION, (long) channel, (long) connection);
}

static inline int ipc_reject_connection(channel_t *channel, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_REJECT_CONNECTION, (long) channel, (long) connection);
}

static inline int ipc_send(channel_t *channel, void *data, size_t size)
{
    return syscall3(SYSCALL_IPC_SEND, (long) channel, (long) data, (long) size);
}

static inline int ipc_receive(channel_t *channel, connection_t *sender, void *data, size_t size)
{
    return syscall4(
        SYSCALL_IPC_RECEIVE, (long) channel, (long) sender, (long) data, (long) size);
}

static inline int ipc_disconnect(channel_t *channel)
{
    return syscall1(SYSCALL_IPC_DISCONNECT, (long) channel);
}
