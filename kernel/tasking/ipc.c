/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/ipc.h>
#include <kernel/timer.h>

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

#define IPC_SHM_MAX_REGIONS 128
#define IPC_SHM_DEFAULT_REGION_CAPACITY 4

typedef struct
{
    uint64_t owner_task_id;
    uint64_t client_task_id;
    uintptr_t phys_addr;
    uintptr_t owner_vaddr;
    uintptr_t client_vaddr;
    size_t size;
    uint64_t flags;
} _ipc_shared_region_t;

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
    channel_id_t id;
    _ipc_connection_t *connections;
    size_t connections_count;
    size_t max_connections;
    uint64_t owner_task_id;
    _ipc_owner_message_t *owner_messages;
    size_t owner_messages_count;
    size_t owner_messages_capacity;
    _ipc_shared_region_t *shared_regions;
    size_t shared_regions_count;
    size_t shared_regions_capacity;
} _ipc_channel_t;

static _ipc_channel_t **_channels;
static size_t _channels_count;
static size_t _channels_capacity;
static channel_id_t _next_channel_id = 1;

static void _remove_connection(_ipc_channel_t *channel, size_t index);

static int _ensure_shared_region_capacity(_ipc_channel_t *channel)
{
    if (channel->shared_regions_count < channel->shared_regions_capacity)
        return 0;

    size_t new_capacity = channel->shared_regions_capacity == 0
                              ? IPC_SHM_DEFAULT_REGION_CAPACITY
                              : channel->shared_regions_capacity * 2;
    _ipc_shared_region_t *new_regions = (_ipc_shared_region_t *)
        krealloc(channel->shared_regions, sizeof(_ipc_shared_region_t) * new_capacity);
    if (!new_regions)
        return -1;

    channel->shared_regions = new_regions;
    channel->shared_regions_capacity = new_capacity;
    return 0;
}

static task_memblock_t *_find_task_memblock(task_t *task, uintptr_t virt_addr, size_t required_size)
{
    if (!task || !task->memory.memblocks || required_size == 0)
        return NULL;

    uintptr_t required_end = virt_addr + required_size;
    if (required_end < virt_addr)
        return NULL;

    for (size_t i = 0; i < task->memory.memblocks_count; i++) {
        task_memblock_t *memblock = &task->memory.memblocks[i];
        uintptr_t block_start = memblock->virt_addr;
        uintptr_t block_end = block_start + memblock->page_count * PAGE_SIZE;
        if (block_end < block_start)
            continue;

        if (virt_addr < block_start || required_end > block_end)
            continue;

        return memblock;
    }

    return NULL;
}

static void _remove_shared_region_at(_ipc_channel_t *channel, size_t index)
{
    if (index >= channel->shared_regions_count)
        return;

    for (size_t i = index + 1; i < channel->shared_regions_count; i++)
        channel->shared_regions[i - 1] = channel->shared_regions[i];

    channel->shared_regions_count--;
}

static void _release_shared_region_entry(_ipc_channel_t *channel, size_t index)
{
    if (index >= channel->shared_regions_count)
        return;

    _ipc_shared_region_t region = channel->shared_regions[index];
    size_t pages = PAGE_UP(region.size) / PAGE_SIZE;

    task_t *owner = task_find_by_id(region.owner_task_id);
    task_t *client = task_find_by_id(region.client_task_id);

    if (client && region.client_vaddr)
        task_unmap(client, region.client_vaddr, pages, false);

    if (region.phys_addr) {
        if (owner && region.owner_vaddr)
            task_unmap(owner, region.owner_vaddr, pages, false);

        pmm_free((void *) region.phys_addr, pages);
    }

    _remove_shared_region_at(channel, index);
}

static void _release_shared_regions_for_task(_ipc_channel_t *channel, uint64_t task_id)
{
    if (!channel)
        return;

    size_t i = 0;
    while (i < channel->shared_regions_count) {
        _ipc_shared_region_t *region = &channel->shared_regions[i];
        if (region->owner_task_id == task_id) {
            _release_shared_region_entry(channel, i);
            continue;
        }
        if (region->client_task_id == task_id) {
            _remove_shared_region_at(channel, i);
            continue;
        }
        i++;
    }
}

static int _enqueue_owner_message(
    _ipc_channel_t *channel, const task_t *sender, const void *data, size_t size)
{
    if (size == 0 || !sender)
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
    if (index >= _channels_capacity)
        return;

    _ipc_channel_t *channel = _channels[index];
    if (channel) {
        if (channel->connections) {
            for (size_t i = 0; i < channel->max_connections; i++) {
                if (channel->connections[i].task_id != 0)
                    _remove_connection(channel, i);
            }
        }

        while (channel->shared_regions_count > 0)
            _release_shared_region_entry(channel, 0);

        if (channel->connections)
            kfree(channel->connections);
        if (channel->owner_messages) {
            for (size_t i = 0; i < channel->owner_messages_count; i++)
                kfree(channel->owner_messages[i].message.data);
            kfree(channel->owner_messages);
        }
        if (channel->shared_regions)
            kfree(channel->shared_regions);

        kfree(channel);
    }

    _channels[index] = NULL;
    if (_channels_count > 0)
        _channels_count--;
}

static _ipc_channel_t *_find_channel(const char *name)
{
    for (size_t i = 0; i < _channels_capacity; i++) {
        _ipc_channel_t *channel = _channels[i];
        if (channel && channel->name[0] != '\0' && strcmp(channel->name, name) == 0)
            return channel;
    }
    return NULL;
}

static _ipc_channel_t *_get_channel_by_id(channel_id_t id)
{
    if (id <= 0 || (size_t) id >= _channels_capacity)
        return NULL;
    return _channels[id];
}

static int _ensure_channel_capacity(channel_id_t required_id)
{
    if (required_id <= 0)
        return -1;

    size_t required_index = (size_t) required_id;
    if (required_index < _channels_capacity)
        return 0;

    size_t old_capacity = _channels_capacity;
    size_t new_capacity = _channels_capacity == 0 ? 4 : _channels_capacity;
    while (new_capacity <= required_index)
        new_capacity *= 2;

    _ipc_channel_t **new_channels
        = (_ipc_channel_t **) krealloc(_channels, sizeof(_ipc_channel_t *) * new_capacity);
    if (!new_channels)
        return -1;

    _channels = new_channels;
    if (new_capacity > old_capacity)
        memset(&_channels[old_capacity], 0, sizeof(_ipc_channel_t *) * (new_capacity - old_capacity));
    _channels_capacity = new_capacity;
    return 0;
}

static _ipc_connection_t *_get_connection_by_task_id(_ipc_channel_t *channel, uint64_t task_id)
{
    if (task_id >= channel->max_connections)
        return NULL;

    _ipc_connection_t *conn = &channel->connections[task_id];
    if (conn->task_id != task_id)
        return NULL;

    return conn;
}

static int _ensure_connection_capacity(_ipc_channel_t *channel, uint64_t task_id)
{
    if (!channel || task_id == 0)
        return -1;

    if (task_id < channel->max_connections)
        return 0;

    size_t old_capacity = channel->max_connections;
    size_t new_capacity = channel->max_connections == 0 ? 4 : channel->max_connections;
    while (new_capacity <= task_id)
        new_capacity *= 2;

    _ipc_connection_t *new_connections = (_ipc_connection_t *)
        krealloc(channel->connections, sizeof(_ipc_connection_t) * new_capacity);
    if (!new_connections)
        return -1;

    channel->connections = new_connections;
    if (new_capacity > old_capacity)
        memset(
            &channel->connections[old_capacity],
            0,
            sizeof(_ipc_connection_t) * (new_capacity - old_capacity));
    channel->max_connections = new_capacity;
    return 0;
}

static _ipc_connection_t *_add_connection(_ipc_channel_t *channel, task_t *task)
{
    if (!channel || !task || task->id == 0)
        return NULL;

    if (_ensure_connection_capacity(channel, task->id) != 0)
        return NULL;

    _ipc_connection_t *conn = &channel->connections[task->id];
    if (conn->task_id != 0)
        return conn;

    memset(conn, 0, sizeof(*conn));
    conn->task_id = task->id;
    conn->state = IPC_CONN_PENDING;
    channel->connections_count++;
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
    if (!channel || !channel->connections || index >= channel->max_connections)
        return;

    _ipc_connection_t *conn = &channel->connections[index];
    if (conn->task_id == 0)
        return;

    _free_messages(conn);
    memset(conn, 0, sizeof(*conn));
    if (channel->connections_count > 0)
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

channel_id_t ipc_new_channel(task_t *task, const char *name)
{
    if (!task || !name)
        return -1;

    if (_find_channel(name))
        return -1;

    if (_ensure_channel_capacity(_next_channel_id) != 0)
        return -1;

    _ipc_channel_t *channel = (_ipc_channel_t *) kmalloc(sizeof(_ipc_channel_t));
    if (!channel)
        return -1;

    memset(channel, 0, sizeof(*channel));
    strncpy(channel->name, name, IPC_CHANNEL_NAME_MAX);
    channel->name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel->owner_task_id = task->id;
    channel->id = _next_channel_id++;

    _channels[channel->id] = channel;
    _channels_count++;
    return channel->id;
}

channel_id_t ipc_connect(task_t *task, const char *name)
{
    if (!task || !name)
        return -1;

    _ipc_channel_t *ipc_channel = _find_channel(name);
    if (!ipc_channel)
        return -1;

    _ipc_connection_t *existing = _get_connection_by_task_id(ipc_channel, task->id);
    if (!existing) {
        if (!_add_connection(ipc_channel, task))
            return -1;
    } else if (existing->state == IPC_CONN_REJECTED) {
        existing->state = IPC_CONN_PENDING;
    }

    return ipc_channel->id;
}

int ipc_poll_connection(task_t *task, channel_id_t channel_id, connection_t *connection)
{
    if (!task || channel_id < 0 || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    for (size_t i = 0; i < ipc_channel->max_connections; i++) {
        _ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->task_id == 0 || conn->state != IPC_CONN_PENDING)
            continue;
        connection->task_id = conn->task_id;
        return 0;
    }

    return 1;
}

int ipc_await_connection(task_t *task, channel_id_t channel_id, connection_t *connection)
{
    if (!task || channel_id < 0 || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id == task->id) {
        while (true) {
            int result = ipc_poll_connection(task, channel_id, connection);
            if (result <= 0)
                return result;
            sleep(1);
        }
    }

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, task->id);
    if (!conn)
        return -1;

    while (true) {
        if (conn->state == IPC_CONN_ACCEPTED) {
            connection->task_id = task->id;
            return 0;
        }
        if (conn->state == IPC_CONN_REJECTED)
            return -1;

        sleep(1);
        conn = _get_connection_by_task_id(ipc_channel, task->id);
        if (!conn)
            return -1;
    }
}

int ipc_accept_connection(task_t *task, channel_id_t channel_id, connection_t *connection)
{
    if (!task || channel_id < 0 || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, connection->task_id);
    if (!conn)
        return -1;

    conn->state = IPC_CONN_ACCEPTED;
    return 0;
}

int ipc_reject_connection(task_t *task, channel_id_t channel_id, connection_t *connection)
{
    if (!task || channel_id < 0 || !connection)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel || (ipc_channel->owner_task_id != 0 && ipc_channel->owner_task_id != task->id))
        return -1;

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, connection->task_id);
    if (!conn)
        return -1;

    _remove_connection(ipc_channel, connection->task_id);
    return 0;
}

int ipc_disconnect(task_t *task, channel_id_t channel_id)
{
    if (!task || channel_id < 0)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id == task->id) {
        _destroy_channel((size_t) channel_id);
        return 0;
    }

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, task->id);
    if (conn) {
        ipc_message_t owner_msg;
        memset(&owner_msg, 0, sizeof(owner_msg));
        owner_msg.type = IPC_OWNER_MSG_TYPE_DISCONNECT;
        owner_msg.sender_task_id = task->id;
        if (ipc_channel->owner_task_id != 0)
            _enqueue_owner_message(ipc_channel, task, &owner_msg, sizeof(owner_msg));

        _remove_connection(ipc_channel, task->id);
        _release_shared_regions_for_task(ipc_channel, task->id);
        return 0;
    }

    return 1;
}

int ipc_send_to(
    task_t *task, channel_id_t channel_id, connection_t *connection, void *data, size_t size)
{
    if (!task || channel_id < 0 || !connection || !data || size == 0)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id != task->id)
        return -1;

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, connection->task_id);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    if (_enqueue_message(conn, data, size) != 0)
        return -1;

    return 0;
}

int ipc_send(task_t *task, channel_id_t channel_id, void *data, size_t size)
{
    if (!task || channel_id < 0 || !data || size == 0)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id != task->id) {
        _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, task->id);
        if (!conn || conn->state != IPC_CONN_ACCEPTED)
            return -1;

        size_t msg_size = sizeof(ipc_message_t) + size;
        ipc_message_t *owner_msg = (ipc_message_t *) kmalloc(msg_size);
        if (!owner_msg)
            return -1;

        memset(owner_msg, 0, msg_size);
        owner_msg->type = IPC_OWNER_MSG_TYPE_DATA;
        owner_msg->sender_task_id = task->id;
        owner_msg->payload.data.size = size;
        memcpy(owner_msg->payload.data.data, data, size);

        int result = _enqueue_owner_message(ipc_channel, task, owner_msg, msg_size);
        kfree(owner_msg);
        if (result != 0)
            return -1;

        return 0;
    }

    for (size_t i = 0; i < ipc_channel->max_connections; i++) {
        _ipc_connection_t *conn = &ipc_channel->connections[i];
        if (conn->task_id == 0 || conn->state != IPC_CONN_ACCEPTED)
            continue;
        if (_enqueue_message(conn, data, size) != 0)
            return -1;
    }

    return 0;
}

void *ipc_share_memory(
    task_t *task, channel_id_t channel_id, connection_t *connection, void *owner_addr, size_t size)
{
    if (!task || channel_id < 0 || !connection || !owner_addr || size == 0)
        return NULL;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return NULL;

    if (ipc_channel->owner_task_id != task->id)
        return NULL;

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, connection->task_id);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return NULL;

    task_t *client = task_find_by_id(connection->task_id);
    if (!client)
        return NULL;

    if (ipc_channel->shared_regions_count >= IPC_SHM_MAX_REGIONS)
        return NULL;

    size_t aligned_size = PAGE_UP(size);
    size_t num_pages = aligned_size / PAGE_SIZE;
    if (num_pages == 0)
        return NULL;

    uintptr_t owner_vaddr = (uintptr_t) owner_addr;
    task_memblock_t *owner_memblock = _find_task_memblock(task, owner_vaddr, aligned_size);
    if (!owner_memblock)
        return NULL;

    uintptr_t offset = owner_vaddr - owner_memblock->virt_addr;
    uintptr_t phys_addr = owner_memblock->phys_addr + offset;

    uintptr_t client_vaddr = task_find_free_vaddr(client, num_pages);
    if (client_vaddr == 0)
        return NULL;

    if (task_map(client, client_vaddr, phys_addr, num_pages, owner_memblock->flags, false) < 0) {
        return NULL;
    }

    if (_ensure_shared_region_capacity(ipc_channel) != 0) {
        task_unmap(client, client_vaddr, num_pages, false);
        return NULL;
    }

    _ipc_shared_region_t *region = &ipc_channel->shared_regions[ipc_channel->shared_regions_count++];
    memset(region, 0, sizeof(*region));
    region->owner_task_id = task->id;
    region->client_task_id = client->id;
    region->phys_addr = phys_addr;
    region->owner_vaddr = owner_vaddr;
    region->client_vaddr = client_vaddr;
    region->size = size;
    region->flags = owner_memblock->flags;

    return (void *) client_vaddr;
}

int ipc_release_shared_memory(task_t *task, channel_id_t channel_id, void *addr)
{
    if (!task || channel_id < 0 || !addr)
        return -1;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    uintptr_t target_addr = (uintptr_t) addr;

    if (ipc_channel->owner_task_id == task->id) {
        for (size_t i = 0; i < ipc_channel->shared_regions_count; i++) {
            _ipc_shared_region_t *region = &ipc_channel->shared_regions[i];
            if (region->owner_vaddr != target_addr)
                continue;
            _release_shared_region_entry(ipc_channel, i);
            return 0;
        }
        return -1;
    }

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, task->id);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    for (size_t i = 0; i < ipc_channel->shared_regions_count; i++) {
        _ipc_shared_region_t *region = &ipc_channel->shared_regions[i];
        if (region->client_task_id != task->id)
            continue;
        if (region->client_vaddr != target_addr)
            continue;
        _release_shared_region_entry(ipc_channel, i);
        return 0;
    }

    return -1;
}

int ipc_receive(task_t *task, channel_id_t channel_id, connection_t *sender, void *data, size_t size)
{
    if (!task || channel_id < 0 || !data || size == 0)
        return -1;

    int bytes_written = 0;

    _ipc_channel_t *ipc_channel = _get_channel_by_id(channel_id);
    if (!ipc_channel)
        return -1;

    if (ipc_channel->owner_task_id == task->id) {
        if (ipc_channel->owner_messages_count == 0)
            return bytes_written;

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

        return bytes_written;
    }

    _ipc_connection_t *conn = _get_connection_by_task_id(ipc_channel, task->id);
    if (!conn || conn->state != IPC_CONN_ACCEPTED)
        return -1;

    if (conn->messages_count == 0)
        return bytes_written;

    size_t required_size = 0;
    for (size_t i = 0; i < conn->messages_count; i++)
        required_size += sizeof(_ipc_batch_entry_t) + conn->messages[i].size;
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

    return bytes_written;
}

void ipc_task_cleanup(task_t *task)
{
    if (!task)
        return;

    for (size_t i = 0; i < _channels_capacity; i++) {
        _ipc_channel_t *channel = _channels[i];
        if (!channel)
            continue;

        if (channel->owner_task_id == task->id) {
            _destroy_channel(i);
            continue;
        }

        _release_shared_regions_for_task(channel, task->id);

        _ipc_connection_t *conn = _get_connection_by_task_id(channel, task->id);
        if (conn) {
            ipc_message_t owner_msg;
            memset(&owner_msg, 0, sizeof(owner_msg));
            owner_msg.type = IPC_OWNER_MSG_TYPE_DISCONNECT;
            owner_msg.sender_task_id = task->id;
            if (channel->owner_task_id != 0)
                _enqueue_owner_message(channel, task, &owner_msg, sizeof(owner_msg));
            _remove_connection(channel, task->id);
        }
    }
}
