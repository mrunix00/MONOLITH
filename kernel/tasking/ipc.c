/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/tasking/ipc.h>

typedef enum {
    IPC_CONN_PENDING = 0,
    IPC_CONN_ACCEPTED = 1,
    IPC_CONN_REJECTED = 2,
} ipc_conn_state_t;

typedef struct
{
    void *data;
    size_t size;
} ipc_message_t;

typedef struct
{
    ipc_message_t message;
    uint64_t sender_task_id;
} ipc_owner_message_t;

typedef struct
{
    connection_t public;
    task_t *task;
    ipc_conn_state_t state;
    ipc_message_t *messages;
    size_t messages_count;
    size_t messages_capacity;
} ipc_connection_t;

typedef struct
{
    channel_t public;
    task_t *owner;
    ipc_connection_t *connections;
    ipc_owner_message_t *owner_messages;
    size_t owner_messages_count;
    size_t owner_messages_capacity;
} ipc_channel_t;

static ipc_channel_t **_channels;
static size_t _channels_count;
static size_t _channels_capacity;

static void _remove_connection(ipc_channel_t *channel, size_t index);

#define IPC_OWNER_DISCONNECT_TYPE 0xFFFFFFFFu

static int _enqueue_owner_message(
    ipc_channel_t *channel, const task_t *sender, const void *data, size_t size)
{
    if (!channel || !data || size == 0 || !sender)
        return -1;

    if (channel->owner_messages_count >= channel->owner_messages_capacity) {
        size_t new_capacity = channel->owner_messages_capacity == 0
                                  ? 4
                                  : channel->owner_messages_capacity * 2;
        ipc_owner_message_t *new_messages = (ipc_owner_message_t *)
            krealloc(channel->owner_messages, sizeof(ipc_owner_message_t) * new_capacity);
        if (!new_messages)
            return -1;
        channel->owner_messages = new_messages;
        channel->owner_messages_capacity = new_capacity;
    }

    ipc_owner_message_t *message = &channel->owner_messages[channel->owner_messages_count++];
    message->message.data = kmalloc(size);
    if (!message->message.data) {
        channel->owner_messages_count--;
        return -1;
    }
    message->message.size = size;
    memcpy(message->message.data, data, size);
    message->sender_task_id = sender->id;
    return 0;
}

static int _dequeue_owner_message(
    ipc_channel_t *channel, connection_t *sender, void *data, size_t size)
{
    if (!channel || !data)
        return -1;

    if (channel->owner_messages_count == 0)
        return 1;

    ipc_owner_message_t *message = &channel->owner_messages[0];
    if (size < message->message.size)
        return -1;

    memcpy(data, message->message.data, message->message.size);
    kfree(message->message.data);
    if (sender)
        sender->task_id = message->sender_task_id;

    for (size_t i = 1; i < channel->owner_messages_count; i++)
        channel->owner_messages[i - 1] = channel->owner_messages[i];

    channel->owner_messages_count--;
    return 0;
}

static void _destroy_channel(size_t index)
{
    if (index >= _channels_count)
        return;

    ipc_channel_t *channel = _channels[index];
    if (channel) {
        while (channel->public.connections_count > 0)
            _remove_connection(channel, 0);

        if (channel->connections)
            kfree(channel->connections);
        if (channel->owner_messages) {
            for (size_t i = 0; i < channel->owner_messages_count; i++) {
                if (channel->owner_messages[i].message.data)
                    kfree(channel->owner_messages[i].message.data);
            }
            kfree(channel->owner_messages);
        }
        if (channel->public.name)
            kfree(channel->public.name);
        kfree(channel);
    }

    for (size_t i = index + 1; i < _channels_count; i++)
        _channels[i - 1] = _channels[i];
    _channels_count--;
}

static ipc_channel_t *_find_channel(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0; i < _channels_count; i++) {
        ipc_channel_t *channel = _channels[i];
        if (channel && channel->public.name && strcmp(channel->public.name, name) == 0)
            return channel;
    }

    return NULL;
}

static int _ensure_channel_capacity(void)
{
    if (_channels_count < _channels_capacity)
        return 0;

    size_t new_capacity = _channels_capacity == 0 ? 4 : _channels_capacity * 2;
    ipc_channel_t **new_channels
        = (ipc_channel_t **) krealloc(_channels, sizeof(ipc_channel_t *) * new_capacity);
    if (!new_channels)
        return -1;

    _channels = new_channels;
    _channels_capacity = new_capacity;
    return 0;
}

static void _sync_channel_handle(channel_t *handle, ipc_channel_t *channel)
{
    if (!handle || !channel)
        return;

    handle->name = channel->public.name;
    handle->connections = (connection_t *) channel->connections;
    handle->connections_count = channel->public.connections_count;
    handle->max_connections = channel->public.max_connections;
}

static ipc_connection_t *_find_connection_by_id(ipc_channel_t *channel, uint64_t task_id)
{
    if (!channel)
        return NULL;

    for (size_t i = 0; i < channel->public.connections_count; i++) {
        ipc_connection_t *conn = &channel->connections[i];
        if (conn->public.task_id == task_id)
            return conn;
    }

    return NULL;
}

static ipc_connection_t *_find_connection_by_task(ipc_channel_t *channel, const task_t *task)
{
    if (!channel || !task)
        return NULL;

    for (size_t i = 0; i < channel->public.connections_count; i++) {
        ipc_connection_t *conn = &channel->connections[i];
        if (conn->task == task)
            return conn;
    }

    return NULL;
}

static int _ensure_connection_capacity(ipc_channel_t *channel)
{
    if (!channel)
        return -1;

    if (channel->public.connections_count < channel->public.max_connections)
        return 0;

    size_t new_capacity = channel->public.max_connections == 0
                              ? 4
                              : channel->public.max_connections * 2;
    ipc_connection_t *new_connections = (ipc_connection_t *)
        krealloc(channel->connections, sizeof(ipc_connection_t) * new_capacity);
    if (!new_connections)
        return -1;

    channel->connections = new_connections;
    channel->public.connections = (connection_t *) new_connections;
    channel->public.max_connections = new_capacity;
    return 0;
}

static ipc_connection_t *_add_connection(ipc_channel_t *channel, task_t *task)
{
    if (!channel || !task)
        return NULL;

    if (_ensure_connection_capacity(channel) != 0)
        return NULL;

    ipc_connection_t *conn = &channel->connections[channel->public.connections_count++];
    memset(conn, 0, sizeof(*conn));
    conn->task = task;
    conn->public.task_id = task->id;
    conn->state = IPC_CONN_PENDING;
    return conn;
}

static void _free_messages(ipc_connection_t *connection)
{
    if (!connection || !connection->messages)
        return;

    for (size_t i = 0; i < connection->messages_count; i++) {
        if (connection->messages[i].data)
            kfree(connection->messages[i].data);
    }

    kfree(connection->messages);
    connection->messages = NULL;
    connection->messages_count = 0;
    connection->messages_capacity = 0;
}

static void _remove_connection(ipc_channel_t *channel, size_t index)
{
    if (!channel || index >= channel->public.connections_count)
        return;

    ipc_connection_t *conn = &channel->connections[index];
    _free_messages(conn);

    for (size_t i = index + 1; i < channel->public.connections_count; i++)
        channel->connections[i - 1] = channel->connections[i];

    channel->public.connections_count--;
}

static int _enqueue_message(ipc_connection_t *connection, const void *data, size_t size)
{
    if (!connection || !data || size == 0)
        return -1;

    if (connection->messages_count >= connection->messages_capacity) {
        size_t new_capacity = connection->messages_capacity == 0
                                  ? 4
                                  : connection->messages_capacity * 2;
        ipc_message_t *new_messages = (ipc_message_t *)
            krealloc(connection->messages, sizeof(ipc_message_t) * new_capacity);
        if (!new_messages)
            return -1;
        connection->messages = new_messages;
        connection->messages_capacity = new_capacity;
    }

    ipc_message_t *message = &connection->messages[connection->messages_count++];
    message->data = kmalloc(size);
    if (!message->data) {
        connection->messages_count--;
        return -1;
    }
    message->size = size;
    memcpy(message->data, data, size);
    return 0;
}

static int _dequeue_message(ipc_connection_t *connection, void *data, size_t size)
{
    if (!connection || !data)
        return -1;

    if (connection->messages_count == 0)
        return 1;

    ipc_message_t *message = &connection->messages[0];
    if (size < message->size)
        return -1;

    memcpy(data, message->data, message->size);
    kfree(message->data);

    for (size_t i = 1; i < connection->messages_count; i++)
        connection->messages[i - 1] = connection->messages[i];

    connection->messages_count--;
    return 0;
}

int broadcast_new(task_t *task, const char *name)
{
    if (!task || !name)
        return -1;

    if (_find_channel(name))
        return -1;

    if (_ensure_channel_capacity() != 0)
        return -1;

    ipc_channel_t *channel = (ipc_channel_t *) kmalloc(sizeof(ipc_channel_t));
    if (!channel)
        return -1;

    memset(channel, 0, sizeof(*channel));
    channel->public.name = strdup(name);
    if (!channel->public.name) {
        kfree(channel);
        return -1;
    }
    channel->owner = task;

    _channels[_channels_count++] = channel;
    return 0;
}

int broadcast_request_connection(task_t *task, const char *name, channel_t *channel)
{
    if (!task || !name)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(name);
    if (!ipc_channel)
        return -1;

    ipc_connection_t *existing = _find_connection_by_task(ipc_channel, task);
    if (!existing) {
        if (!_add_connection(ipc_channel, task))
            return -1;
    } else if (existing->state == IPC_CONN_REJECTED) {
        existing->state = IPC_CONN_PENDING;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int broadcast_wait_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || !channel->name || !connection)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner && ipc_channel->owner != task))
        return -1;

    for (size_t i = 0; i < ipc_channel->public.connections_count; i++) {
        ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->state != IPC_CONN_PENDING)
            continue;
        connection->task_id = conn->public.task_id;
        _sync_channel_handle(channel, ipc_channel);
        return 0;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 1;
}

int broadcast_accept_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || !channel->name || !connection)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner && ipc_channel->owner != task))
        return -1;

    ipc_connection_t *conn = _find_connection_by_id(ipc_channel, connection->task_id);
    if (!conn)
        return -1;

    conn->state = IPC_CONN_ACCEPTED;
    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int broadcast_reject_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || !channel->name || !connection)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner && ipc_channel->owner != task))
        return -1;

    for (size_t i = 0; i < ipc_channel->public.connections_count; i++) {
        ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->public.task_id == connection->task_id) {
            _remove_connection(ipc_channel, i);
            _sync_channel_handle(channel, ipc_channel);
            return 0;
        }
    }

    return -1;
}

int broadcast_disconnect(task_t *task, channel_t *channel)
{
    if (!task || !channel || !channel->name)
        return -1;

    for (size_t i = 0; i < _channels_count; i++) {
        ipc_channel_t *ipc_channel = _channels[i];
        if (!ipc_channel || !ipc_channel->public.name)
            continue;
        if (strcmp(ipc_channel->public.name, channel->name) != 0)
            continue;

        if (ipc_channel->owner == task) {
            _destroy_channel(i);
            return 0;
        }

        for (size_t j = 0; j < ipc_channel->public.connections_count; j++) {
            ipc_connection_t *conn = &ipc_channel->connections[j];
            if (conn->task == task) {
                _remove_connection(ipc_channel, j);
                _sync_channel_handle(channel, ipc_channel);
                return 0;
            }
        }

        _sync_channel_handle(channel, ipc_channel);
        return 1;
    }

    return -1;
}

int broadcast_send(task_t *task, channel_t *channel, void *data, size_t size)
{
    if (!task || !channel || !channel->name || !data || size == 0)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner != task) {
        ipc_connection_t *conn = _find_connection_by_task(ipc_channel, task);
        if (!conn || conn->state != IPC_CONN_ACCEPTED)
            return -1;
        if (_enqueue_owner_message(ipc_channel, task, data, size) != 0)
            return -1;
        _sync_channel_handle(channel, ipc_channel);
        return 0;
    }

    for (size_t i = 0; i < ipc_channel->public.connections_count; i++) {
        ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->state != IPC_CONN_ACCEPTED)
            continue;
        if (_enqueue_message(conn, data, size) != 0)
            return -1;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int broadcast_receive(task_t *task, channel_t *channel, connection_t *sender, void *data, size_t size)
{
    if (!task || !channel || !channel->name || !data || size == 0)
        return -1;

    ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner == task) {
        int result = _dequeue_owner_message(ipc_channel, sender, data, size);
        _sync_channel_handle(channel, ipc_channel);
        return result;
    }

    ipc_connection_t *conn = _find_connection_by_task(ipc_channel, task);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    int result = _dequeue_message(conn, data, size);
    if (result == 0 && sender)
        sender->task_id = ipc_channel->owner ? ipc_channel->owner->id : 0;
    _sync_channel_handle(channel, ipc_channel);
    return result;
}

void ipc_task_cleanup(task_t *task)
{
    if (!task)
        return;

    size_t i = 0;
    while (i < _channels_count) {
        ipc_channel_t *channel = _channels[i];
        if (!channel) {
            i++;
            continue;
        }

        if (channel->owner == task) {
            _destroy_channel(i);
            continue;
        }

        bool removed = false;
        for (size_t j = 0; j < channel->public.connections_count; j++) {
            ipc_connection_t *conn = &channel->connections[j];
            if (conn->task == task) {
                uint32_t disconnect_type = IPC_OWNER_DISCONNECT_TYPE;
                if (channel->owner)
                    _enqueue_owner_message(channel, task, &disconnect_type, sizeof(disconnect_type));
                _remove_connection(channel, j);
                removed = true;
                break;
            }
        }

        if (!removed)
            i++;
    }
}
