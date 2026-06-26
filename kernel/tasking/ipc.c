/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/tasking/ipc.h>
#include <kernel/timer.h>

typedef enum {
    IPC_CONN_PENDING = 0,
    IPC_CONN_ACCEPTED = 1,
    IPC_CONN_REJECTED = 2,
} _ipc_conn_state_t;

typedef struct _ipc_packet
{
    ipc_channel_packet_header_t header;
    rsrc_t *resource;
    unsigned char payload[];
} _ipc_packet_t;

typedef struct
{
    _ipc_packet_t **packets;
    size_t count;
    size_t capacity;
} _ipc_packet_queue_t;

typedef struct
{
    uint64_t task_id;
    _ipc_conn_state_t state;
    _ipc_packet_queue_t queue;
} _ipc_connection_t;

typedef struct
{
    char name[RSRC_NAME_MAX_LEN];
    uint64_t id;
    rsrc_t *resource;
    _ipc_connection_t *connections;
    size_t max_connections;
    uint64_t owner_task_id;
    _ipc_packet_queue_t owner_queue;
} _ipc_channel_t;

static _ipc_channel_t **_channels;
static size_t _channels_capacity;
static uint64_t _next_channel_id = 1;
static bool _ipc_resource_initialized = false;
static const rsrc_ops_t _channel_resource_ops;

static _ipc_connection_t *_get_connection_by_task_id(_ipc_channel_t *channel, uint64_t task_id);

static _ipc_packet_queue_t *_queue_for_task(_ipc_channel_t *channel, task_t *task)
{
    if (channel == NULL || task == NULL)
        return NULL;
    if (channel->owner_task_id == task->id)
        return &channel->owner_queue;

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, task->id);
    if (connection == NULL || connection->state != IPC_CONN_ACCEPTED)
        return NULL;
    return &connection->queue;
}

static void _channel_detach_resource(rsrc_t *resource)
{
    if (resource == NULL || resource->node == NULL || resource->node->parent == NULL)
        return;

    rsrc_node_t *node = resource->node;
    rsrc_node_t *parent = node->parent;
    if (parent->first_child == node) {
        parent->first_child = node->next_sibling;
    } else {
        rsrc_node_t *current = parent->first_child;
        while (current != NULL && current->next_sibling != node)
            current = current->next_sibling;
        if (current != NULL)
            current->next_sibling = node->next_sibling;
    }

    resource->node = NULL;
    kfree(node);
}

static int _ensure_channel_capacity(uint64_t required_id)
{
    if (required_id <= 0)
        return -1;

    size_t required_index = (size_t) required_id;
    if (required_index < _channels_capacity)
        return 0;

    size_t old_capacity = _channels_capacity;
    size_t new_capacity = old_capacity == 0 ? 4 : old_capacity;
    while (new_capacity <= required_index)
        new_capacity *= 2;

    _ipc_channel_t **new_channels
        = (_ipc_channel_t **) krealloc(_channels, sizeof(_ipc_channel_t *) * new_capacity);
    if (new_channels == NULL)
        return -1;

    _channels = new_channels;
    memset(&_channels[old_capacity], 0, sizeof(_ipc_channel_t *) * (new_capacity - old_capacity));
    _channels_capacity = new_capacity;
    return 0;
}

static _ipc_connection_t *_get_connection_by_task_id(_ipc_channel_t *channel, uint64_t task_id)
{
    if (channel == NULL || task_id >= channel->max_connections)
        return NULL;

    _ipc_connection_t *connection = &channel->connections[task_id];
    return connection->task_id == task_id ? connection : NULL;
}

static int _ensure_connection_capacity(_ipc_channel_t *channel, uint64_t task_id)
{
    if (channel == NULL || task_id == 0)
        return -1;

    if (task_id < channel->max_connections)
        return 0;

    size_t old_capacity = channel->max_connections;
    size_t new_capacity = old_capacity == 0 ? 4 : old_capacity;
    while (new_capacity <= task_id)
        new_capacity *= 2;

    _ipc_connection_t *connections = (_ipc_connection_t *)
        krealloc(channel->connections, sizeof(_ipc_connection_t) * new_capacity);
    if (connections == NULL)
        return -1;

    channel->connections = connections;
    memset(
        &channel->connections[old_capacity],
        0,
        sizeof(_ipc_connection_t) * (new_capacity - old_capacity));
    channel->max_connections = new_capacity;
    return 0;
}

static int _ensure_queue_capacity(_ipc_packet_queue_t *queue)
{
    if (queue == NULL)
        return -1;
    if (queue->count < queue->capacity)
        return 0;

    size_t new_capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
    _ipc_packet_t **packets
        = (_ipc_packet_t **) krealloc(queue->packets, sizeof(_ipc_packet_t *) * new_capacity);
    if (packets == NULL)
        return -1;

    queue->packets = packets;
    queue->capacity = new_capacity;
    return 0;
}

static void _free_packet(_ipc_packet_t *packet)
{
    if (packet == NULL)
        return;
    if (packet->resource != NULL && packet->resource->refcount > 0)
        packet->resource->refcount--;
    kfree(packet);
}

static void _free_queue(_ipc_packet_queue_t *queue)
{
    if (queue == NULL)
        return;
    for (size_t i = 0; i < queue->count; i++)
        _free_packet(queue->packets[i]);
    kfree(queue->packets);
    queue->packets = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

static _ipc_packet_t *_new_packet(
    uint32_t type,
    uint64_t connection_id,
    uint64_t sender_task_id,
    uint64_t flags,
    const void *payload,
    uint64_t payload_len,
    rsrc_t *resource)
{
    _ipc_packet_t *packet = (_ipc_packet_t *) kmalloc(sizeof(_ipc_packet_t) + payload_len);
    if (packet == NULL)
        return NULL;

    memset(packet, 0, sizeof(_ipc_packet_t) + payload_len);
    packet->header.type = type;
    packet->header.connection_id = connection_id;
    packet->header.sender_task_id = sender_task_id;
    packet->header.payload_len = payload_len;
    packet->header.flags = flags;
    packet->resource = resource;

    if (resource != NULL)
        resource->refcount++;
    if (payload_len > 0 && payload != NULL)
        memcpy(packet->payload, payload, payload_len);
    return packet;
}

static int _enqueue_packet(_ipc_packet_queue_t *queue, _ipc_packet_t *packet)
{
    if (queue == NULL || packet == NULL || _ensure_queue_capacity(queue) != 0)
        return -1;
    queue->packets[queue->count++] = packet;
    return 0;
}

static void _dequeue_packet(_ipc_packet_queue_t *queue, size_t index)
{
    if (queue == NULL || index >= queue->count)
        return;
    _free_packet(queue->packets[index]);
    for (size_t i = index + 1; i < queue->count; i++)
        queue->packets[i - 1] = queue->packets[i];
    queue->count--;
}

static _ipc_connection_t *_add_connection(_ipc_channel_t *channel, task_t *task)
{
    if (channel == NULL || task == NULL || _ensure_connection_capacity(channel, task->id) != 0)
        return NULL;

    _ipc_connection_t *connection = &channel->connections[task->id];
    if (connection->task_id == 0) {
        memset(connection, 0, sizeof(*connection));
        connection->task_id = task->id;
        connection->state = IPC_CONN_PENDING;
    }
    return connection;
}

static void _remove_connection(_ipc_channel_t *channel, uint64_t task_id)
{
    if (channel == NULL || task_id >= channel->max_connections)
        return;

    _ipc_connection_t *connection = &channel->connections[task_id];
    if (connection->task_id == 0)
        return;

    _free_queue(&connection->queue);
    memset(connection, 0, sizeof(*connection));
}

static _ipc_channel_t *_create_channel(task_t *task, const char *name)
{
    if (task == NULL || !ipc_init())
        return NULL;
    if (_ensure_channel_capacity(_next_channel_id) != 0)
        return NULL;

    _ipc_channel_t *channel = (_ipc_channel_t *) kmalloc(sizeof(_ipc_channel_t));
    if (channel == NULL)
        return NULL;

    memset(channel, 0, sizeof(*channel));
    strncpy(channel->name, name == NULL ? "anon" : name, RSRC_NAME_MAX_LEN);
    channel->name[RSRC_NAME_MAX_LEN - 1] = '\0';
    channel->id = _next_channel_id++;
    channel->owner_task_id = task->id;

    channel->resource = rsmgr_new_resource(RSRC_DOMAIN_CHANNEL, channel->name);
    if (channel->resource == NULL) {
        kfree(channel);
        return NULL;
    }

    channel->resource->header.type = RSRC_TYPE_RESOURCE;
    channel->resource->ops = &_channel_resource_ops;
    channel->resource->type_state = channel;

    if (name != NULL) {
        rsrc_status_t result = rsmgr_attach_resource_at_path(
            RSRC_DOMAIN_CHANNEL, name, channel->resource, &_channel_resource_ops);
        if (result != RSRC_STATUS_OK) {
            kfree(channel->resource);
            kfree(channel);
            return NULL;
        }
    }

    _channels[channel->id] = channel;
    return channel;
}

static int _connect_channel(task_t *task, _ipc_channel_t *channel)
{
    if (task == NULL || channel == NULL)
        return -1;
    if (channel->owner_task_id == task->id)
        return 0;

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, task->id);
    if (connection == NULL) {
        connection = _add_connection(channel, task);
        if (connection == NULL)
            return -1;
    } else if (connection->state == IPC_CONN_REJECTED) {
        connection->state = IPC_CONN_PENDING;
    } else if (connection->state == IPC_CONN_ACCEPTED) {
        return 0;
    }

    _ipc_packet_t *packet
        = _new_packet(IPC_CHANNEL_PACKET_CONNECTION_REQUEST, task->id, task->id, 0, NULL, 0, NULL);
    if (packet == NULL)
        return -1;
    if (_enqueue_packet(&channel->owner_queue, packet) != 0) {
        _free_packet(packet);
        return -1;
    }
    return 0;
}

static int _poll_channel_connection(task_t *task, _ipc_channel_t *channel, connection_t *connection)
{
    if (task == NULL || channel == NULL || connection == NULL || channel->owner_task_id != task->id)
        return -1;

    for (size_t i = 0; i < channel->max_connections; i++) {
        _ipc_connection_t *candidate = &channel->connections[i];
        if (candidate->task_id == 0 || candidate->state != IPC_CONN_PENDING)
            continue;
        *connection = candidate->task_id;
        return 0;
    }

    return 1;
}

static int _await_channel_connection(task_t *task, _ipc_channel_t *channel, connection_t *connection)
{
    if (task == NULL || channel == NULL || connection == NULL)
        return -1;

    if (channel->owner_task_id == task->id) {
        while (true) {
            int result = _poll_channel_connection(task, channel, connection);
            if (result <= 0)
                return result;
            sleep(1);
        }
    }

    _ipc_connection_t *candidate = _get_connection_by_task_id(channel, task->id);
    if (candidate == NULL)
        return -1;

    while (true) {
        if (candidate->state == IPC_CONN_ACCEPTED) {
            *connection = task->id;
            return 0;
        }
        if (candidate->state == IPC_CONN_REJECTED)
            return -1;
        sleep(1);
        candidate = _get_connection_by_task_id(channel, task->id);
        if (candidate == NULL)
            return -1;
    }
}

static int _accept_channel_connection(task_t *task, _ipc_channel_t *channel, connection_t *connection)
{
    if (task == NULL || channel == NULL || connection == NULL || channel->owner_task_id != task->id)
        return -1;

    _ipc_connection_t *candidate = _get_connection_by_task_id(channel, *connection);
    if (candidate == NULL)
        return -1;

    candidate->state = IPC_CONN_ACCEPTED;
    return 0;
}

static int _reject_channel_connection(task_t *task, _ipc_channel_t *channel, connection_t *connection)
{
    if (task == NULL || channel == NULL || connection == NULL || channel->owner_task_id != task->id)
        return -1;

    _ipc_connection_t *candidate = _get_connection_by_task_id(channel, *connection);
    if (candidate == NULL)
        return -1;

    candidate->state = IPC_CONN_REJECTED;
    _remove_connection(channel, *connection);
    return 0;
}

static int _queue_message_packet(
    _ipc_packet_queue_t *queue,
    uint64_t connection_id,
    uint64_t sender_task_id,
    const void *payload,
    uint64_t payload_len)
{
    _ipc_packet_t *packet = _new_packet(
        IPC_CHANNEL_PACKET_MESSAGE, connection_id, sender_task_id, 0, payload, payload_len, NULL);
    if (packet == NULL)
        return -1;
    if (_enqueue_packet(queue, packet) != 0) {
        _free_packet(packet);
        return -1;
    }
    return 0;
}

static int _queue_object_packet(
    _ipc_packet_queue_t *queue,
    uint64_t connection_id,
    uint64_t sender_task_id,
    uint64_t flags,
    rsrc_t *resource)
{
    _ipc_packet_t *packet = _new_packet(
        IPC_CHANNEL_PACKET_OBJECT, connection_id, sender_task_id, flags, NULL, 0, resource);
    if (packet == NULL)
        return -1;
    if (_enqueue_packet(queue, packet) != 0) {
        _free_packet(packet);
        return -1;
    }
    return 0;
}

static int _send_channel(task_t *task, _ipc_channel_t *channel, const void *data, size_t size)
{
    if (task == NULL || channel == NULL || data == NULL || size == 0)
        return -1;

    if (channel->owner_task_id == task->id) {
        for (size_t i = 0; i < channel->max_connections; i++) {
            _ipc_connection_t *connection = &channel->connections[i];
            if (connection->task_id == 0 || connection->state != IPC_CONN_ACCEPTED)
                continue;
            if (_queue_message_packet(&connection->queue, connection->task_id, task->id, data, size)
                != 0)
                return -1;
        }
        return 0;
    }

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, task->id);
    if (connection == NULL || connection->state != IPC_CONN_ACCEPTED)
        return -1;
    return _queue_message_packet(&channel->owner_queue, task->id, task->id, data, size);
}

static int _send_channel_to(
    task_t *task, _ipc_channel_t *channel, uint64_t connection_id, const void *data, size_t size)
{
    if (task == NULL || channel == NULL || data == NULL || size == 0
        || channel->owner_task_id != task->id)
        return -1;

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, connection_id);
    if (connection == NULL || connection->state != IPC_CONN_ACCEPTED)
        return -1;

    return _queue_message_packet(&connection->queue, connection_id, task->id, data, size);
}

static int _send_channel_resource(
    task_t *task,
    _ipc_channel_t *channel,
    uint64_t connection_id,
    rsrc_handle_t handle,
    uint64_t flags)
{
    if (task == NULL || channel == NULL || handle < 0)
        return -1;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&task->handle_table, (int) handle);
    if (entry == NULL || entry->resource == NULL)
        return -1;

    if (channel->owner_task_id == task->id) {
        _ipc_connection_t *connection = _get_connection_by_task_id(channel, connection_id);
        if (connection == NULL || connection->state != IPC_CONN_ACCEPTED)
            return -1;
        return _queue_object_packet(
            &connection->queue, connection_id, task->id, flags, entry->resource);
    }

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, task->id);
    if (connection == NULL || connection->state != IPC_CONN_ACCEPTED)
        return -1;
    return _queue_object_packet(&channel->owner_queue, task->id, task->id, flags, entry->resource);
}

static int _write_packet(_ipc_packet_t *packet, task_t *receiver, void *buffer, uint64_t buffer_len)
{
    if (packet == NULL || receiver == NULL || buffer == NULL
        || buffer_len < sizeof(ipc_channel_packet_header_t))
        return -1;

    ipc_channel_recv_out_t *out = (ipc_channel_recv_out_t *) buffer;
    out->header = packet->header;

    if (packet->header.type == IPC_CHANNEL_PACKET_OBJECT) {
        if (buffer_len < sizeof(ipc_channel_recv_out_t) + sizeof(ipc_channel_object_payload_t))
            return -1;

        int handle = rsmgr_handle_table_alloc(&receiver->handle_table, packet->resource);
        if (handle < 0)
            return -1;

        ipc_channel_object_payload_t payload = {
            .resource = handle,
            .resource_type = packet->resource != NULL ? packet->resource->header.domain_id : 0,
            .rights = 0,
        };
        memcpy(out->payload, &payload, sizeof(payload));
        out->header.payload_len = sizeof(payload);
        return (int) (sizeof(ipc_channel_packet_header_t) + sizeof(payload));
    }

    if (sizeof(ipc_channel_recv_out_t) + packet->header.payload_len > buffer_len)
        return -1;

    if (packet->header.payload_len > 0)
        memcpy(out->payload, packet->payload, packet->header.payload_len);
    return (int) (sizeof(ipc_channel_packet_header_t) + packet->header.payload_len);
}

static int _receive_packet(
    task_t *task, _ipc_channel_t *channel, _ipc_packet_queue_t *queue, void *buffer, size_t size)
{
    if (task == NULL || channel == NULL || queue == NULL || buffer == NULL || size == 0)
        return -1;
    if (queue->count == 0)
        return 0;

    int written = _write_packet(queue->packets[0], task, buffer, size);
    if (written < 0)
        return -1;

    _dequeue_packet(queue, 0);
    return written;
}

static int _receive_message(
    task_t *task, _ipc_channel_t *channel, _ipc_packet_queue_t *queue, void *buffer, size_t size)
{
    if (task == NULL || channel == NULL || queue == NULL || buffer == NULL || size == 0)
        return -1;
    if (queue->count == 0)
        return 0;

    _ipc_packet_t *packet = queue->packets[0];
    if (packet->header.type != IPC_CHANNEL_PACKET_MESSAGE || packet->header.payload_len > size)
        return -1;

    memcpy(buffer, packet->payload, packet->header.payload_len);
    int written = (int) packet->header.payload_len;
    _dequeue_packet(queue, 0);
    return written;
}

static int _disconnect_channel(task_t *task, _ipc_channel_t *channel)
{
    if (task == NULL || channel == NULL)
        return -1;

    if (channel->owner_task_id == task->id) {
        uint64_t id = channel->id;
        if (channel->resource != NULL) {
            _channel_detach_resource(channel->resource);
            channel->resource->type_state = NULL;
        }

        for (size_t i = 0; i < channel->max_connections; i++) {
            if (channel->connections[i].task_id != 0)
                _remove_connection(channel, channel->connections[i].task_id);
        }
        _free_queue(&channel->owner_queue);
        kfree(channel->connections);
        kfree(channel);
        _channels[id] = NULL;
        return 0;
    }

    _ipc_connection_t *connection = _get_connection_by_task_id(channel, task->id);
    if (connection == NULL)
        return 1;

    _ipc_packet_t *packet
        = _new_packet(IPC_CHANNEL_PACKET_DISCONNECT, task->id, task->id, 0, NULL, 0, NULL);
    if (packet != NULL)
        _enqueue_packet(&channel->owner_queue, packet);
    _remove_connection(channel, task->id);
    return 0;
}

static int _channel_connect_resource(task_t *task, rsrc_t *resource)
{
    if (task == NULL || resource == NULL || resource->type_state == NULL)
        return -1;
    return _connect_channel(task, (_ipc_channel_t *) resource->type_state);
}

static rsrc_status_t _channel_domain_open(
    rsrc_domain_t *domain, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (domain == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *node = path[0] == '\0' || strcmp(path, "/") == 0
                            ? domain->root_node
                            : rsmgr_get_relative_path(domain->root_node, path);
    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    if (node->resource->header.type != RSRC_TYPE_COLLECTION
        && _channel_connect_resource(task_get_current(), node->resource) < 0) {
        return RSRC_ERROR_PERMISSION_DENIED;
    }

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_domain_lookup(
    rsrc_node_t *parent, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (parent == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *node = rsmgr_get_relative_path(parent, path);
    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    if (node->resource->header.type != RSRC_TYPE_COLLECTION
        && _channel_connect_resource(task_get_current(), node->resource) < 0) {
        return RSRC_ERROR_PERMISSION_DENIED;
    }

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_list_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written)
{
    (void) handle_state;
    return rsmgr_list_collection_entries(
        resource, cursor, buffer, buffer_len, out_next_cursor, out_bytes_written);
}

static rsrc_status_t _channel_read_op(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || resource->type_state == NULL || buffer == NULL || out_bytes_read == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    (void) handle_state;
    task_t *current = task_get_current();
    _ipc_channel_t *channel = (_ipc_channel_t *) resource->type_state;
    if (current == NULL)
        return RSRC_ERROR;

    _ipc_packet_queue_t *queue = _queue_for_task(channel, current);
    if (queue == NULL)
        return RSRC_ERROR_PERMISSION_DENIED;

    int result = _receive_message(current, channel, queue, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_WOULD_BLOCK;

    *out_bytes_read = (uint64_t) result;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_write_op(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || resource->type_state == NULL || buffer == NULL
        || out_bytes_written == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }
    (void) handle_state;

    if (_send_channel(task_get_current(), (_ipc_channel_t *) resource->type_state, buffer, buffer_len)
        != 0)
        return RSRC_ERROR_IO;

    *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_poll_op(
    rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events)
{
    if (resource == NULL || resource->type_state == NULL || out_ready_events == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    (void) handle_state;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    _ipc_channel_t *channel = (_ipc_channel_t *) resource->type_state;
    _ipc_packet_queue_t *queue = _queue_for_task(channel, current);
    if (queue == NULL)
        return RSRC_ERROR_PERMISSION_DENIED;

    uint64_t ready = 0;
    if ((requested_events & RSRC_POLL_READ) != 0 && queue->count > 0)
        ready |= RSRC_POLL_READ;
    if ((requested_events & RSRC_POLL_WRITE) != 0)
        ready |= RSRC_POLL_WRITE;

    *out_ready_events = ready;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_describe_op(rsrc_t *resource, rsrc_info_t *out_info)
{
    if (resource == NULL || out_info == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    _ipc_channel_t *channel = (_ipc_channel_t *) resource->type_state;
    if (channel == NULL)
        return RSRC_STATUS_OK;

    out_info->channel.channel_id = (uint64_t) channel->id;
    out_info->channel.owner_task_id = channel->owner_task_id;
    return RSRC_STATUS_OK;
}

rsrc_status_t ipc_channel_create(const char *name, rsrc_t **out_resource)
{
    task_t *current = task_get_current();

    if (current == NULL || out_resource == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (name != NULL) {
        size_t name_len = strlen(name);
        if (name_len == 0 || name_len >= RSRC_NAME_MAX_LEN)
            return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _ipc_channel_t *channel = _create_channel(current, name);
    if (channel == NULL)
        return RSRC_ERROR_NO_MEMORY;

    *out_resource = channel->resource;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _channel_command_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t command_id,
    const void *in,
    uint64_t in_len,
    void *out,
    uint64_t out_len,
    uint64_t *out_bytes_written)
{
    task_t *current = task_get_current();
    if (resource == NULL || current == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (out_bytes_written != NULL)
        *out_bytes_written = 0;
    (void) handle_state;

    _ipc_channel_t *channel = (_ipc_channel_t *) resource->type_state;
    if (channel == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    switch (command_id) {
    case IPC_CHANNEL_COMMAND_SEND_MESSAGE: {
        const ipc_channel_send_message_in_t *request = (const ipc_channel_send_message_in_t *) in;
        if (request == NULL || in_len < sizeof(*request)
            || in_len < sizeof(*request) + request->payload_len) {
            return RSRC_ERROR_INVALID_ARGUMENT;
        }

        if (channel->owner_task_id == current->id) {
            return _send_channel_to(
                       current,
                       channel,
                       request->connection_id,
                       request->payload,
                       request->payload_len)
                           == 0
                       ? RSRC_STATUS_OK
                       : RSRC_ERROR_IO;
        }
        return _send_channel(current, channel, request->payload, request->payload_len) == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_IO;
    }
    case IPC_CHANNEL_COMMAND_SEND_OBJECT: {
        const ipc_channel_send_object_in_t *request = (const ipc_channel_send_object_in_t *) in;
        if (request == NULL || in_len < sizeof(*request))
            return RSRC_ERROR_INVALID_ARGUMENT;
        return _send_channel_resource(
                   current, channel, request->connection_id, request->resource, request->flags)
                       == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_IO;
    }
    case IPC_CHANNEL_COMMAND_RECV: {
        if (out == NULL || out_len < sizeof(ipc_channel_packet_header_t))
            return RSRC_ERROR_INVALID_ARGUMENT;

        _ipc_packet_queue_t *queue = _queue_for_task(channel, current);
        if (queue == NULL)
            return RSRC_ERROR_PERMISSION_DENIED;

        int result = _receive_packet(current, channel, queue, out, out_len);
        if (result < 0)
            return RSRC_ERROR_WOULD_BLOCK;
        if (out_bytes_written != NULL)
            *out_bytes_written = (uint64_t) result;
        return RSRC_STATUS_OK;
    }
    case IPC_CHANNEL_COMMAND_POLL_CONNECTION:
        if (out == NULL || out_len < sizeof(connection_t))
            return RSRC_ERROR_INVALID_ARGUMENT;
        return _poll_channel_connection(current, channel, (connection_t *) out) == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_WOULD_BLOCK;
    case IPC_CHANNEL_COMMAND_WAIT_CONNECTION:
        if (out == NULL || out_len < sizeof(connection_t))
            return RSRC_ERROR_INVALID_ARGUMENT;
        return _await_channel_connection(current, channel, (connection_t *) out) == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_IO;
    case IPC_CHANNEL_COMMAND_ACCEPT_CONNECTION:
        if (out == NULL || out_len < sizeof(connection_t))
            return RSRC_ERROR_INVALID_ARGUMENT;
        return _accept_channel_connection(current, channel, (connection_t *) out) == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_NOT_FOUND;
    case IPC_CHANNEL_COMMAND_REJECT_CONNECTION:
        if (out == NULL || out_len < sizeof(connection_t))
            return RSRC_ERROR_INVALID_ARGUMENT;
        return _reject_channel_connection(current, channel, (connection_t *) out) == 0
                   ? RSRC_STATUS_OK
                   : RSRC_ERROR_NOT_FOUND;
    case IPC_CHANNEL_COMMAND_DISCONNECT:
        return _disconnect_channel(current, channel) == 0 ? RSRC_STATUS_OK : RSRC_ERROR_NOT_FOUND;
    default:
        return RSRC_ERROR_NOT_SUPPORTED;
    }
}

static const rsrc_ops_t _channel_resource_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = _channel_describe_op,
    .seek = NULL,
    .list = _channel_list_op,
    .read = _channel_read_op,
    .write = _channel_write_op,
    .mmap = NULL,
    .poll = _channel_poll_op,
    .create = NULL,
    .remove = NULL,
    .control = _channel_command_op,
};

bool ipc_init()
{
    if (_ipc_resource_initialized)
        return true;
    if (!rsmgr_init())
        return false;

    rsrc_ops_t domain_ops = {
        .open = _channel_domain_open,
        .lookup = _channel_domain_lookup,
        .dup_handle = NULL,
        .close_handle = NULL,
        .destroy = NULL,
        .describe = NULL,
        .seek = NULL,
        .list = NULL,
        .read = NULL,
        .write = NULL,
        .mmap = NULL,
        .poll = NULL,
        .remove = NULL,
        .control = NULL,
    };
    if (!rsmgr_init_domain(RSRC_DOMAIN_CHANNEL, "channel", &domain_ops))
        return false;

    rsrc_t *root = rsmgr_new_resource(RSRC_DOMAIN_CHANNEL, "/");
    if (root == NULL)
        return false;
    root->header.type = RSRC_TYPE_COLLECTION;
    root->ops = &_channel_resource_ops;
    root->type_state = NULL;
    if (rsmgr_set_domain_root(RSRC_DOMAIN_CHANNEL, root) == NULL)
        return false;

    _ipc_resource_initialized = true;
    return true;
}

void ipc_task_cleanup(task_t *task)
{
    if (task == NULL)
        return;

    for (size_t i = 0; i < _channels_capacity; i++) {
        _ipc_channel_t *channel = _channels[i];
        if (channel == NULL)
            continue;

        if (channel->owner_task_id == task->id) {
            _disconnect_channel(task, channel);
            continue;
        }

        if (_get_connection_by_task_id(channel, task->id) != NULL)
            _disconnect_channel(task, channel);
    }
}
