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

typedef int64_t channel_id_t;

#define IPC_SHM_FLAG_RW (1 << 0)
#define IPC_SHM_FLAG_EXEC (1 << 1)

typedef enum {
    IPC_OWNER_MSG_TYPE_DATA = 0,
    IPC_OWNER_MSG_TYPE_SHM_REQUEST,
    IPC_OWNER_MSG_TYPE_DISCONNECT,
} ipc_msg_type_t;

typedef struct
{
    uint64_t client_task_id;
    uintptr_t owner_address;
    uintptr_t client_address;
    size_t size;
    uint64_t flags;
} ipc_shm_request_t;

typedef struct
{
    size_t size;
    unsigned char data[];
} ipc_msg_data_t;

typedef struct
{
    ipc_msg_type_t type;
    uint64_t sender_task_id;
    union {
        ipc_shm_request_t shm_request;
        ipc_msg_data_t data;
    } payload;
} ipc_message_t;

/*
 * Create a new IPC channel.
 * Returns channel ID on success, -1 on failure.
 */
channel_id_t ipc_new_channel(task_t *, const char *name);

/*
 * Connect a client task to a named channel.
 * Returns channel ID on success, -1 on failure.
 */
channel_id_t ipc_connect(task_t *, const char *name);

/*
 * Poll for a pending connection on a channel.
 * Returns 0 and sets connection on success, 1 if none, -1 on failure.
 */
int ipc_poll_connection(task_t *, channel_id_t, connection_t *);

/*
 * Await a pending connection on a channel.
 * Returns 0 and sets connection on success, -1 on failure.
 */
int ipc_await_connection(task_t *, channel_id_t, connection_t *);

/*
 * Accept a pending connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_accept_connection(task_t *, channel_id_t, connection_t *);

/*
 * Reject a pending connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_reject_connection(task_t *, channel_id_t, connection_t *);

/*
 * Disconnect from a pending connection (client side).
 * Returns 0 on success, -1 on failure.
 */
int ipc_disconnect_connection(task_t *, channel_id_t, connection_t *);

/*
 * Disconnect from a channel or destroy it.
 * Returns 0 on success, -1 on failure.
 */
int ipc_disconnect(task_t *, channel_id_t);

/*
 * Send data from the channel owner to a specific connection.
 * Returns 0 on success, -1 on failure.
 */
int ipc_send_to(task_t *, channel_id_t, connection_t *, void *data, size_t size);

/*
 * Send data from a client to the owner, or broadcast from owner to all clients.
 * Returns 0 on success, -1 on failure.
 */
int ipc_send(task_t *, channel_id_t, void *data, size_t size);

/*
 * Share an owner-mapped region with a specific accepted client connection.
 * Returns the mapped client virtual address on success, NULL on failure.
 */
void *ipc_share_memory(task_t *, channel_id_t, connection_t *, void *owner_addr, size_t size);

/*
 * Release a previously mapped shared memory region by virtual address.
 * Returns 0 on success, -1 on failure.
 */
int ipc_release_shared_memory(task_t *, channel_id_t, void *addr);

/*
 * Receive all queued messages for the calling task into a buffer.
 * Returns number of bytes written on success, -1 on failure.
 */
int ipc_receive(task_t *, channel_id_t, connection_t *, void *data, size_t size);

/*
 * Cleanup IPC resources for a task that is exiting.
 * task: task being cleaned up.
 */
void ipc_task_cleanup(task_t *task);
