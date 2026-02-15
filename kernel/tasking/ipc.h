/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/tasking/task.h>
#include <stddef.h>

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
 * Returns 0 on success, -1 on failure.
 */
int ipc_new_channel(task_t *, const char *name);

/*
 * Connect a client task to a named channel.
 * Returns 0 on success, -1 on failure.
 */
int ipc_connect(task_t *, const char *, channel_t *);

/*
 * Await a pending connection on a channel.
 * Returns 0 and sets connection on success, -1 on failure.
 */
int ipc_await_connection(task_t *, channel_t *, connection_t *);

/*
 * Accept a pending connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_accept_connection(task_t *, channel_t *, connection_t *);

/*
 * Reject a pending connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_reject_connection(task_t *, channel_t *, connection_t *);

/*
 * Disconnect from a pending connection (client side).
 * Returns 0 on success, -1 on failure.
 */
int ipc_disconnect_connection(task_t *, channel_t *, connection_t *);

/*
 * Disconnect from a channel or destroy it.
 * Returns 0 on success, -1 on failure.
 */
int ipc_disconnect(task_t *, channel_t *);

/*
 * Send data from the channel owner to a specific connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_send_to(task_t *, channel_t *, connection_t *, void *data, size_t size);

/*
 * Send data from a client to the owner, or broadcast from owner to all clients.
 * Returns 0 on success, -1 on failure.
 */
int ipc_send(task_t *, channel_t *, void *data, size_t size);

/*
 * Receive all queued messages for the calling task into a buffer.
 * Returns number of bytes written on success, -1 on failure.
 */
int ipc_receive(task_t *, channel_t *, connection_t *, void *data, size_t size);

/*
 * Cleanup IPC resources for a task that is exiting.
 * task: task being cleaned up.
 */
void ipc_task_cleanup(task_t *task);
