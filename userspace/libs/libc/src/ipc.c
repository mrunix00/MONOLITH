/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <ipc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct
{
    size_t size;
    uint64_t sender_task_id;
} _ipc_batch_entry_t;

typedef struct
{
    char *data;
    size_t capacity;
    size_t length;
    size_t offset;
    channel_id_t channel_id;
} ipc_receive_cache_t;

static ipc_receive_cache_t _ipc_cache;

#ifdef TEST_ENV
typedef int (*ipc_syscall4_fn)(long num, long arg1, long arg2, long arg3, long arg4);
ipc_syscall4_fn ipc_test_syscall4;
#endif

static int _ipc_syscall4(long num, long arg1, long arg2, long arg3, long arg4)
{
#ifdef TEST_ENV
    if (ipc_test_syscall4)
        return ipc_test_syscall4(num, arg1, arg2, arg3, arg4);
#endif
    return (int) syscall4(num, arg1, arg2, arg3, arg4);
}

static void _ipc_cache_reset(void)
{
    _ipc_cache.length = 0;
    _ipc_cache.offset = 0;
}

#ifdef TEST_ENV
void ipc_test_reset_cache(void)
{
    _ipc_cache_reset();
    _ipc_cache.channel_id = -1;
}
#endif

static void _ipc_cache_set_channel(channel_id_t channel_id)
{
    _ipc_cache.channel_id = channel_id;
}

static int _ipc_cache_matches_channel(channel_id_t channel_id)
{
    return _ipc_cache.channel_id == channel_id;
}

static int _ipc_cache_ensure_capacity(size_t min_capacity)
{
    if (_ipc_cache.capacity >= min_capacity)
        return 0;

    size_t new_capacity = _ipc_cache.capacity == 0 ? min_capacity : _ipc_cache.capacity;
    while (new_capacity < min_capacity)
        new_capacity *= 2;

    char *new_data = (char *) realloc(_ipc_cache.data, new_capacity);
    if (!new_data)
        return -1;

    _ipc_cache.data = new_data;
    _ipc_cache.capacity = new_capacity;
    return 0;
}

int ipc_receive(channel_id_t channel_id, connection_t *sender, void *data, size_t size)
{
    if (channel_id < 0 || !data || size == 0)
        return -1;

    if (!_ipc_cache_matches_channel(channel_id)) {
        _ipc_cache_reset();
        _ipc_cache_set_channel(channel_id);
    }

    if (_ipc_cache.offset >= _ipc_cache.length) {
        _ipc_cache_reset();
        size_t min_capacity = size + sizeof(_ipc_batch_entry_t);
        if (_ipc_cache_ensure_capacity(min_capacity) != 0)
            return -1;

        connection_t dummy_sender = {0};
        connection_t *sender_ptr = sender ? sender : &dummy_sender;

        int result = _ipc_syscall4(
            SYSCALL_IPC_RECEIVE_ALL,
            (long) channel_id,
            (long) sender_ptr,
            (long) _ipc_cache.data,
            (long) _ipc_cache.capacity);

        if (result == 1 || result == 0)
            return 1;
        if (result < 0)
            return -1;

        _ipc_cache.length = (size_t) result;
        _ipc_cache.offset = 0;
    }

    if (_ipc_cache.length - _ipc_cache.offset < sizeof(_ipc_batch_entry_t))
        return -1;

    _ipc_batch_entry_t *entry = (_ipc_batch_entry_t *) (_ipc_cache.data + _ipc_cache.offset);
    size_t entry_total = sizeof(_ipc_batch_entry_t) + entry->size;

    if (_ipc_cache.length - _ipc_cache.offset < entry_total)
        return -1;

    if (entry->size > size)
        return -1;

    if (sender)
        sender->task_id = entry->sender_task_id;

    memcpy(data, entry + 1, entry->size);

    _ipc_cache.offset += entry_total;
    if (_ipc_cache.offset >= _ipc_cache.length)
        _ipc_cache_reset();

    return 0;
}
