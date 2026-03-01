/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define IPC_CHANNEL_NAME_MAX 64

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

typedef struct
{
    uint64_t task_id;
} connection_t;

typedef int64_t channel_id_t;

/*
 * Create a new IPC channel.
 * name: unique channel name.
 * Returns channel ID on success, -1 on failure.
 */
static inline channel_id_t ipc_new_channel(const char *name)
{
    return (channel_id_t) syscall1(SYSCALL_IPC_NEW, (long) name);
}

/*
 * Connect to a named channel.
 * name: channel name to connect to.
 * Returns channel ID on success, -1 on failure.
 */
static inline channel_id_t ipc_connect(const char *name)
{
    return (channel_id_t) syscall1(SYSCALL_IPC_REQUEST_CONNECTION, (long) name);
}

/*
 * Await a pending connection on a channel (owner side).
 * channel_id: channel ID to check.
 * connection: output pending connection (task_id set on success).
 */
static inline int ipc_await_connection(channel_id_t channel_id, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_WAIT_CONNECTION, (long) channel_id, (long) connection);
}

/*
 * Accept a pending connection (owner side).
 * channel_id: channel ID.
 * connection: connection to accept (task_id identifies client).
 */
static inline int ipc_accept_connection(channel_id_t channel_id, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_ACCEPT_CONNECTION, (long) channel_id, (long) connection);
}

/*
 * Reject a pending connection (owner side).
 * channel_id: channel ID.
 * connection: connection to reject (task_id identifies client).
 */
static inline int ipc_reject_connection(channel_id_t channel_id, connection_t *connection)
{
    return syscall2(SYSCALL_IPC_REJECT_CONNECTION, (long) channel_id, (long) connection);
}

/*
 * Send data from the channel owner to a specific connection.
 * channel_id: channel ID.
 * connection: target connection/client.
 * data: message payload.
 * size: payload size in bytes.
 */
static inline int ipc_send_to(channel_id_t channel_id, connection_t *connection, void *data, size_t size)
{
    return syscall4(
        SYSCALL_IPC_SEND_TO, (long) channel_id, (long) connection, (long) data, (long) size);
}

/*
 * Send data from a client to the owner, or broadcast from owner to all clients.
 * channel_id: channel ID.
 * data: message payload.
 * size: payload size in bytes.
 */
static inline int ipc_send(channel_id_t channel_id, void *data, size_t size)
{
    return syscall3(SYSCALL_IPC_SEND, (long) channel_id, (long) data, (long) size);
}

/*
 * Request a shared memory region from the channel owner (client side).
 * channel_id: channel ID.
 * size: requested shared memory size in bytes.
 * flags: IPC_SHM_FLAG_* flags.
 * out_addr: output mapped address on success.
 */
int ipc_request_shared_memory(channel_id_t channel_id, size_t size, uint64_t flags, void **out_addr);

/*
 * Release a previously requested shared memory region.
 * channel_id: channel ID.
 * addr: mapped address returned earlier by ipc_request_shared_memory.
 */
static inline int ipc_release_shared_memory(channel_id_t channel_id, void *addr)
{
    return syscall2(SYSCALL_IPC_RELEASE_SHM, (long) channel_id, (long) addr);
}

/*
 * Receive data for the calling task (owner or client).
 * channel_id: channel ID.
 * sender: output sender info (task_id).
 * data: receive buffer.
 * size: buffer size in bytes.
 */
int ipc_receive(channel_id_t channel_id, connection_t *sender, void *data, size_t size);

/*
 * Disconnect from a channel (client) or destroy it (owner).
 * channel_id: channel ID.
 */
static inline int ipc_disconnect(channel_id_t channel_id)
{
    return syscall1(SYSCALL_IPC_DISCONNECT, (long) channel_id);
}
