/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/tasking/pipe.h>

typedef struct
{
    uint8_t *buffer;
    uint64_t size;
    uint64_t capacity;
} _pipe_t;

static bool _pipe_initialized = false;
static const rsrc_ops_t _pipe_ops;

static rsrc_status_t _pipe_domain_open(
    rsrc_domain_t *domain, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (domain == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL
        || domain->root_node == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *node = path[0] == '\0' || strcmp(path, "/") == 0
                            ? domain->root_node
                            : rsmgr_get_relative_path(domain->root_node, path);
    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _pipe_domain_lookup(
    rsrc_node_t *parent, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (parent == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *node = rsmgr_get_relative_path(parent, path);
    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _pipe_list_op(
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

static rsrc_status_t _pipe_ensure_capacity(_pipe_t *pipe, uint64_t required)
{
    if (pipe == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (required <= pipe->capacity)
        return RSRC_STATUS_OK;

    uint64_t new_capacity = pipe->capacity == 0 ? 256 : pipe->capacity;
    while (new_capacity < required)
        new_capacity *= 2;

    uint8_t *new_buffer = (uint8_t *) krealloc(pipe->buffer, new_capacity);
    if (new_buffer == NULL)
        return RSRC_ERROR_NO_MEMORY;

    pipe->buffer = new_buffer;
    pipe->capacity = new_capacity;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _pipe_read_op(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || buffer == NULL || out_bytes_read == NULL
        || resource->header.type != RSRC_TYPE_RESOURCE || resource->type_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    (void) handle_state;

    _pipe_t *pipe = (_pipe_t *) resource->type_state;
    if (pipe->size == 0)
        return RSRC_ERROR_WOULD_BLOCK;

    uint64_t bytes_read = buffer_len < pipe->size ? buffer_len : pipe->size;
    memcpy(buffer, pipe->buffer, bytes_read);

    if (bytes_read < pipe->size) {
        for (uint64_t i = bytes_read; i < pipe->size; i++)
            pipe->buffer[i - bytes_read] = pipe->buffer[i];
    }
    pipe->size -= bytes_read;

    *out_bytes_read = bytes_read;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _pipe_write_op(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || buffer == NULL || out_bytes_written == NULL
        || resource->header.type != RSRC_TYPE_RESOURCE || resource->type_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    (void) handle_state;

    _pipe_t *pipe = (_pipe_t *) resource->type_state;
    rsrc_status_t result = _pipe_ensure_capacity(pipe, pipe->size + buffer_len);
    if (result != RSRC_STATUS_OK)
        return result;

    memcpy(pipe->buffer + pipe->size, buffer, buffer_len);
    pipe->size += buffer_len;
    *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _pipe_poll_op(
    rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events)
{
    if (resource == NULL || out_ready_events == NULL || resource->type_state == NULL
        || resource->header.type != RSRC_TYPE_RESOURCE)
        return RSRC_ERROR_INVALID_ARGUMENT;
    (void) handle_state;

    _pipe_t *pipe = (_pipe_t *) resource->type_state;
    uint64_t ready = 0;
    if ((requested_events & RSRC_POLL_READ) != 0 && pipe->size > 0)
        ready |= RSRC_POLL_READ;
    if ((requested_events & RSRC_POLL_WRITE) != 0)
        ready |= RSRC_POLL_WRITE;

    *out_ready_events = ready;
    return RSRC_STATUS_OK;
}

rsrc_status_t pipe_create(const char *name, rsrc_t **out_resource)
{
    if (out_resource == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (name != NULL) {
        size_t name_len = strlen(name);
        if (name_len == 0 || name_len >= RSRC_NAME_MAX_LEN)
            return RSRC_ERROR_INVALID_ARGUMENT;
    }
    if (!pipe_domain_init())
        return RSRC_ERROR;

    rsrc_t *resource = rsmgr_new_resource(RSRC_DOMAIN_PIPE, name == NULL ? "anon" : name);
    if (resource == NULL)
        return RSRC_ERROR_NO_MEMORY;

    _pipe_t *pipe = (_pipe_t *) kmalloc(sizeof(_pipe_t));
    if (pipe == NULL) {
        kfree(resource);
        return RSRC_ERROR_NO_MEMORY;
    }

    memset(pipe, 0, sizeof(*pipe));
    resource->header.type = RSRC_TYPE_RESOURCE;
    resource->ops = &_pipe_ops;
    resource->type_state = pipe;

    if (name != NULL) {
        rsrc_status_t result
            = rsmgr_attach_resource_at_path(RSRC_DOMAIN_PIPE, name, resource, &_pipe_ops);
        if (result != RSRC_STATUS_OK) {
            kfree(pipe);
            kfree(resource);
            return result;
        }
    }

    *out_resource = resource;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _pipe_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = NULL,
    .list = _pipe_list_op,
    .read = _pipe_read_op,
    .write = _pipe_write_op,
    .mmap = NULL,
    .poll = _pipe_poll_op,
    .create = NULL,
    .remove = NULL,
    .control = NULL,
};

bool pipe_domain_init(void)
{
    if (_pipe_initialized)
        return true;

    if (!rsmgr_init())
        return false;

    rsrc_ops_t domain_ops = {
        .open = _pipe_domain_open,
        .lookup = _pipe_domain_lookup,
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
        .create = NULL,
        .remove = NULL,
        .control = NULL,
    };
    if (!rsmgr_init_domain(RSRC_DOMAIN_PIPE, "pipe", &domain_ops))
        return false;

    rsrc_t *root = rsmgr_new_resource(RSRC_DOMAIN_PIPE, "/");
    if (root == NULL)
        return false;

    root->header.type = RSRC_TYPE_COLLECTION;
    root->ops = &_pipe_ops;
    root->type_state = NULL;
    if (rsmgr_set_domain_root(RSRC_DOMAIN_PIPE, root) == NULL)
        return false;

    _pipe_initialized = true;
    return true;
}
