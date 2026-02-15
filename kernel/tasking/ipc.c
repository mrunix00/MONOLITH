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
} _ipc_conn_state_t;

typedef struct
{
    void *data;
    size_t size;
} _ipc_message_t;

typedef struct
{
    size_t size;
    uint64_t sender_task_id;
} _ipc_batch_entry_t;

typedef struct
{
    _ipc_message_t message;
    uint64_t sender_task_id;
} _ipc_owner_message_t;

typedef struct
{
    uint64_t task_id;
    _ipc_conn_state_t state;
    _ipc_message_t *messages;
    size_t messages_count;
    size_t messages_capacity;
} _ipc_connection_t;

typedef struct
{
    char name[IPC_CHANNEL_NAME_MAX];
    _ipc_connection_t *connections;
    size_t connections_count;
    size_t max_connections;
    uint64_t owner_task_id;
    _ipc_owner_message_t *owner_messages;
    size_t owner_messages_count;
    size_t owner_messages_capacity;
} _ipc_channel_t;

static _ipc_channel_t **_channels;
static size_t _channels_count;
static size_t _channels_capacity;

static void _remove_connection(_ipc_channel_t *channel, size_t index);

#define IPC_OWNER_DISCONNECT_TYPE 0xFFFFFFFFu

static int _enqueue_owner_message(
    _ipc_channel_t *channel, const task_t *sender, const void *data, size_t size)
{
    if (!channel || !data || size == 0 || !sender)
        return -1;

    if (channel->owner_messages_count >= channel->owner_messages_capacity) {
        size_t new_capacity = channel->owner_messages_capacity == 0
                                  ? 4
                                  : channel->owner_messages_capacity * 2;
        _ipc_owner_message_t *new_messages = (_ipc_owner_message_t *)
            krealloc(channel->owner_messages, sizeof(_ipc_owner_message_t) * new_capacity);
        if (!new_messages)
            return -1;
        channel->owner_messages = new_messages;
        channel->owner_messages_capacity = new_capacity;
    }

    _ipc_owner_message_t *message = &channel->owner_messages[channel->owner_messages_count++];
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

static void _destroy_channel(size_t index)
{
    if (index >= _channels_count)
        return;

    _ipc_channel_t *channel = _channels[index];
    if (channel) {
        while (channel->connections_count > 0)
            _remove_connection(channel, 0);

        if (channel->connections)
            kfree(channel->connections);
        if (channel->owner_messages) {
            for (size_t i = 0; i < channel->owner_messages_count; i++)
                kfree(channel->owner_messages[i].message.data);
            kfree(channel->owner_messages);
        }

        kfree(channel);
    }

    for (size_t i = index + 1; i < _channels_count; i++)
        _channels[i - 1] = _channels[i];
    _channels_count--;
}

static _ipc_channel_t *_find_channel(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0; i < _channels_count; i++) {
        _ipc_channel_t *channel = _channels[i];
        if (channel && channel->name[0] != '\0' && strcmp(channel->name, name) == 0)
            return channel;
    }

    return NULL;
}

static int _ensure_channel_capacity(void)
{
    if (_channels_count < _channels_capacity)
        return 0;

    size_t new_capacity = _channels_capacity == 0 ? 4 : _channels_capacity * 2;
    _ipc_channel_t **new_channels
        = (_ipc_channel_t **) krealloc(_channels, sizeof(_ipc_channel_t *) * new_capacity);
    if (!new_channels)
        return -1;

    _channels = new_channels;
    _channels_capacity = new_capacity;
    return 0;
}

static void _sync_channel_handle(channel_t *handle, _ipc_channel_t *channel)
{
    if (!handle || !channel)
        return;

    strncpy(handle->name, channel->name, IPC_CHANNEL_NAME_MAX);
    handle->name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    handle->owner_task_id = channel->owner_task_id;
}

static _ipc_connection_t *_find_connection_by_id(_ipc_channel_t *channel, uint64_t task_id)
{
    if (!channel)
        return NULL;

    for (size_t i = 0; i < channel->connections_count; i++) {
        _ipc_connection_t *conn = &channel->connections[i];
        if (conn->task_id == task_id)
            return conn;
    }

    return NULL;
}

static _ipc_connection_t *_find_connection_by_task(_ipc_channel_t *channel, const task_t *task)
{
    if (!channel || !task)
        return NULL;

    for (size_t i = 0; i < channel->connections_count; i++) {
        _ipc_connection_t *conn = &channel->connections[i];
        if (conn->task_id == task->id)
            return conn;
    }

    return NULL;
}

static int _ensure_connection_capacity(_ipc_channel_t *channel)
{
    if (!channel)
        return -1;

    if (channel->connections_count < channel->max_connections)
        return 0;

    size_t new_capacity = channel->max_connections == 0 ? 4 : channel->max_connections * 2;
    _ipc_connection_t *new_connections = (_ipc_connection_t *)
        krealloc(channel->connections, sizeof(_ipc_connection_t) * new_capacity);
    if (!new_connections)
        return -1;

    channel->connections = new_connections;
    channel->max_connections = new_capacity;
    return 0;
}

static _ipc_connection_t *_add_connection(_ipc_channel_t *channel, task_t *task)
{
    if (!channel || !task)
        return NULL;

    if (_ensure_connection_capacity(channel) != 0)
        return NULL;

    _ipc_connection_t *conn = &channel->connections[channel->connections_count++];
    memset(conn, 0, sizeof(*conn));
    conn->task_id = task->id;
    conn->state = IPC_CONN_PENDING;
    return conn;
}

static void _free_messages(_ipc_connection_t *connection)
{
    if (!connection || !connection->messages)
        return;

    for (size_t i = 0; i < connection->messages_count; i++)
        kfree(connection->messages[i].data);

    kfree(connection->messages);
    connection->messages = NULL;
    connection->messages_count = 0;
    connection->messages_capacity = 0;
}

static void _remove_connection(_ipc_channel_t *channel, size_t index)
{
    if (!channel || index >= channel->connections_count)
        return;

    _ipc_connection_t *conn = &channel->connections[index];
    _free_messages(conn);

    for (size_t i = index + 1; i < channel->connections_count; i++)
        channel->connections[i - 1] = channel->connections[i];

    channel->connections_count--;
}

static int _enqueue_message(_ipc_connection_t *connection, const void *data, size_t size)
{
    if (!connection || !data || size == 0)
        return -1;

    if (connection->messages_count >= connection->messages_capacity) {
        size_t new_capacity = connection->messages_capacity == 0
                                  ? 4
                                  : connection->messages_capacity * 2;
        _ipc_message_t *new_messages = (_ipc_message_t *)
            krealloc(connection->messages, sizeof(_ipc_message_t) * new_capacity);
        if (!new_messages)
            return -1;
        connection->messages = new_messages;
        connection->messages_capacity = new_capacity;
    }

    _ipc_message_t *message = &connection->messages[connection->messages_count++];
    message->data = kmalloc(size);
    if (!message->data) {
        connection->messages_count--;
        return -1;
    }
    message->size = size;
    memcpy(message->data, data, size);
    return 0;
}

int ipc_new_channel(task_t *task, const char *name)
{
    if (!task || !name)
        return -1;

    if (_find_channel(name))
        return -1;

    if (_ensure_channel_capacity() != 0)
        return -1;

    _ipc_channel_t *channel = (_ipc_channel_t *) kmalloc(sizeof(_ipc_channel_t));
    if (!channel)
        return -1;

    memset(channel, 0, sizeof(*channel));
    strncpy(channel->name, name, IPC_CHANNEL_NAME_MAX);
    channel->name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel->owner_task_id = task->id;

    _channels[_channels_count++] = channel;
    return 0;
}

int ipc_connect(task_t *task, const char *name, channel_t *channel)
{
    if (!task || !name)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(name);
    if (!ipc_channel)
        return -1;

    _ipc_connection_t *existing = _find_connection_by_task(ipc_channel, task);
    if (!existing) {
        if (!_add_connection(ipc_channel, task))
            return -1;
    } else if (existing->state == IPC_CONN_REJECTED) {
        existing->state = IPC_CONN_PENDING;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int ipc_await_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || channel->name[0] == '\0' || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    for (size_t i = 0; i < ipc_channel->connections_count; i++) {
        _ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->state != IPC_CONN_PENDING)
            continue;
        connection->task_id = conn->task_id;
        _sync_channel_handle(channel, ipc_channel);
        return 0;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 1;
}

int ipc_accept_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || channel->name[0] == '\0' || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    _ipc_connection_t *conn = _find_connection_by_id(ipc_channel, connection->task_id);
    if (!conn)
        return -1;

    conn->state = IPC_CONN_ACCEPTED;
    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int ipc_reject_connection(task_t *task, channel_t *channel, connection_t *connection)
{
    if (!task || !channel || channel->name[0] == '\0' || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    for (size_t i = 0; i < ipc_channel->connections_count; i++) {
        _ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->task_id == connection->task_id) {
            _remove_connection(ipc_channel, i);
            _sync_channel_handle(channel, ipc_channel);
            return 0;
        }
    }

    return -1;
}

int ipc_disconnect(task_t *task, channel_t *channel)
{
    if (!task || !channel || channel->name[0] == '\0')
        return -1;

    for (size_t i = 0; i < _channels_count; i++) {
        _ipc_channel_t *ipc_channel = _channels[i];
        if (!ipc_channel || ipc_channel->name[0] == '\0')
            continue;
        if (strcmp(ipc_channel->name, channel->name) != 0)
            continue;

        if (ipc_channel->owner_task_id == task->id) {
            _destroy_channel(i);
            return 0;
        }

        for (size_t j = 0; j < ipc_channel->connections_count; j++) {
            _ipc_connection_t *conn = &ipc_channel->connections[j];
            if (conn->task_id == task->id) {
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

int ipc_send_to(task_t *task, channel_t *channel, connection_t *connection, void *data, size_t size)
{
    if (!task || !channel || channel->name[0] == '\0' || !connection || !data || size == 0)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id != task->id)
        return -1;

    _ipc_connection_t *conn = _find_connection_by_id(ipc_channel, connection->task_id);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    if (_enqueue_message(conn, data, size) != 0)
        return -1;

    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int ipc_send(task_t *task, channel_t *channel, void *data, size_t size)
{
    if (!task || !channel || channel->name[0] == '\0' || !data || size == 0)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id != task->id) {
        _ipc_connection_t *conn = _find_connection_by_task(ipc_channel, task);
        if (!conn || conn->state != IPC_CONN_ACCEPTED)
            return -1;
        if (_enqueue_owner_message(ipc_channel, task, data, size) != 0)
            return -1;
        _sync_channel_handle(channel, ipc_channel);
        return 0;
    }

    for (size_t i = 0; i < ipc_channel->connections_count; i++) {
        _ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->state != IPC_CONN_ACCEPTED)
            continue;
        if (_enqueue_message(conn, data, size) != 0)
            return -1;
    }

    _sync_channel_handle(channel, ipc_channel);
    return 0;
}

int ipc_receive(task_t *task, channel_t *channel, connection_t *sender, void *data, size_t size)
{
    if (!task || !channel || channel->name[0] == '\0' || !data || size == 0)
        return -1;

    int bytes_written = 0;

    _ipc_channel_t *ipc_channel = _find_channel(channel->name);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id == task->id) {
        if (ipc_channel->owner_messages_count == 0) {
            _sync_channel_handle(channel, ipc_channel);
            return 1;
        }

        size_t required_size = 0;
        for (size_t i = 0; i < ipc_channel->owner_messages_count; i++) {
            required_size += sizeof(_ipc_batch_entry_t)
                             + ipc_channel->owner_messages[i].message.size;
        }
        if (required_size > size)
            return -1;

        if (sender)
            sender->task_id = ipc_channel->owner_messages[0].sender_task_id;

        while (ipc_channel->owner_messages_count > 0) {
            _ipc_owner_message_t *msg = &ipc_channel->owner_messages[0];
            size_t entry_size = sizeof(_ipc_batch_entry_t) + msg->message.size;

            _ipc_batch_entry_t *entry = (_ipc_batch_entry_t *) ((char *) data + bytes_written);
            entry->size = msg->message.size;
            entry->sender_task_id = msg->sender_task_id;
            memcpy(entry + 1, msg->message.data, msg->message.size);
            bytes_written += entry_size;

            kfree(msg->message.data);
            for (size_t i = 1; i < ipc_channel->owner_messages_count; i++)
                ipc_channel->owner_messages[i - 1] = ipc_channel->owner_messages[i];
            ipc_channel->owner_messages_count--;
        }

        _sync_channel_handle(channel, ipc_channel);
        return bytes_written;
    }

    _ipc_connection_t *conn = _find_connection_by_task(ipc_channel, task);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    if (conn->messages_count == 0) {
        _sync_channel_handle(channel, ipc_channel);
        return 1;
    }

    size_t required_size = 0;
    for (size_t i = 0; i < conn->messages_count; i++) {
        required_size += sizeof(_ipc_batch_entry_t) + conn->messages[i].size;
    }
    if (required_size > size)
        return -1;

    if (sender)
        sender->task_id = ipc_channel->owner_task_id;

    while (conn->messages_count > 0) {
        _ipc_message_t *msg = &conn->messages[0];
        size_t entry_size = sizeof(_ipc_batch_entry_t) + msg->size;

        _ipc_batch_entry_t *entry = (_ipc_batch_entry_t *) ((char *) data + bytes_written);
        entry->size = msg->size;
        entry->sender_task_id = ipc_channel->owner_task_id;
        memcpy(entry + 1, msg->data, msg->size);
        bytes_written += entry_size;

        kfree(msg->data);
        for (size_t i = 1; i < conn->messages_count; i++)
            conn->messages[i - 1] = conn->messages[i];
        conn->messages_count--;
    }

    _sync_channel_handle(channel, ipc_channel);
    return bytes_written;
}

void ipc_task_cleanup(task_t *task)
{
    if (!task)
        return;

    size_t i = 0;
    while (i < _channels_count) {
        _ipc_channel_t *channel = _channels[i];
        if (!channel) {
            i++;
            continue;
        }

        if (channel->owner_task_id == task->id) {
            _destroy_channel(i);
            continue;
        }

        bool removed = false;
        for (size_t j = 0; j < channel->connections_count; j++) {
            _ipc_connection_t *conn = &channel->connections[j];
            if (conn->task_id == task->id) {
                uint32_t disconnect_type = IPC_OWNER_DISCONNECT_TYPE;
                if (channel->owner_task_id != 0)
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
