/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/tasking/task.h>
#include <stddef.h>

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

int broadcast_new(task_t *, const char *name);
int broadcast_request_connection(task_t *, const char *, channel_t *);
int broadcast_wait_connection(task_t *, channel_t *, connection_t *);
int broadcast_accept_connection(task_t *, channel_t *, connection_t *);
int broadcast_reject_connection(task_t *, channel_t *, connection_t *);
int broadcast_disconnect(task_t *, channel_t *);
int broadcast_send(task_t *, channel_t *, void *data, size_t size);
int broadcast_receive(task_t *, channel_t *, connection_t *, void *data, size_t size);
void ipc_task_cleanup(task_t *task);
