/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/rsmgr/rsmgr.h>
#include <kernel/tasking/task_domain.h>

static bool _task_domain_initialized = false;
static rsrc_node_t *_task_domain_root_node = NULL;

static const rsrc_ops_t _task_collection_ops;

static void _task_domain_detach_node(rsrc_node_t *node)
{
    if (node == NULL || node->parent == NULL)
        return;

    rsrc_node_t *parent = node->parent;
    if (parent->first_child == node) {
        parent->first_child = node->next_sibling;
    } else {
        rsrc_node_t *prev = parent->first_child;
        while (prev != NULL && prev->next_sibling != node)
            prev = prev->next_sibling;
        if (prev != NULL)
            prev->next_sibling = node->next_sibling;
    }
    node->parent = NULL;
    node->next_sibling = NULL;
}

static int _list_task_entries(rsrc_t *resource, uint64_t cursor, void *buffer, uint64_t buffer_len)
{
    if (resource == NULL || resource->header.type != RSRC_TYPE_COLLECTION || resource->node == NULL
        || buffer == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    rsrc_node_t *current = resource->node->first_child;
    uint32_t buffer_offset = 0;
    size_t current_offset = 0;

    while (current != NULL) {
        size_t name_len = strlen(current->resource->header.name);
        size_t entry_size = sizeof(uint32_t) + name_len + 1;

        if (current_offset + entry_size <= cursor) {
            current_offset += entry_size;
            current = current->next_sibling;
            continue;
        }

        if (buffer_offset + entry_size > buffer_len)
            break;

        memcpy((uint8_t *) buffer + buffer_offset, &entry_size, sizeof(uint32_t));
        memcpy(
            (uint8_t *) buffer + buffer_offset + sizeof(uint32_t),
            current->resource->header.name,
            name_len + 1);

        buffer_offset += (uint32_t) entry_size;
        current_offset += entry_size;
        current = current->next_sibling;
    }

    return (int) buffer_offset;
}

static rsrc_status_t _task_collection_list_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || handle_state != NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    int result = _list_task_entries(resource, cursor, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_IO;

    *out_bytes_written = (uint64_t) result;
    if (out_next_cursor != NULL)
        *out_next_cursor = cursor + (uint64_t) result;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _task_collection_describe_op(rsrc_t *resource, rsrc_info_t *out_info)
{
    if (resource == NULL || out_info == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *task = (task_t *) resource->type_state;
    if (task == NULL)
        return RSRC_STATUS_OK;

    uint64_t child_count = 0;
    for (rsrc_node_t *child = resource->node == NULL ? NULL : resource->node->first_child;
         child != NULL;
         child = child->next_sibling) {
        child_count++;
    }

    out_info->task.task_id = task->id;
    out_info->task.parent_task_id = task->parent == NULL ? 0 : task->parent->id;
    out_info->task.child_count = child_count;
    out_info->task.exiting = task->exiting;

    strncpy(out_info->task.path, task->path, RSRC_PATH_MAX_LEN);
    out_info->task.path[RSRC_PATH_MAX_LEN - 1] = '\0';
    strcpy(out_info->task.name, task->name);
    return RSRC_STATUS_OK;
}

static rsrc_status_t _task_domain_open(
    rsrc_domain_t *domain, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (domain == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL
        || domain->root_node == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    rsrc_node_t *node = NULL;
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
        node = domain->root_node;
    else
        node = rsmgr_get_relative_path(domain->root_node, path);

    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _task_domain_lookup(
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

static rsrc_status_t _task_collection_remove_op(
    rsrc_t *resource, void *handle_state, const char *name)
{
    (void) handle_state;
    if (resource == NULL || name == NULL || resource->node == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *child = resource->node->first_child;
    while (child != NULL) {
        if (strcmp(child->resource->header.name, name) == 0)
            break;
        child = child->next_sibling;
    }
    if (child == NULL || child->resource == NULL)
        return RSRC_ERROR_NOT_FOUND;

    task_t *task = (task_t *) child->resource->type_state;
    if (task == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    _task_domain_detach_node(child);

    task_remove(task);

    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _task_collection_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = _task_collection_describe_op,
    .seek = NULL,
    .list = _task_collection_list_op,
    .read = NULL,
    .write = NULL,
    .mmap = NULL,
    .poll = NULL,
    .create = NULL,
    .remove = _task_collection_remove_op,
    .control = NULL,
};

bool task_domain_init(void)
{
    if (_task_domain_initialized)
        return true;
    if (!rsmgr_init())
        return false;

    rsrc_ops_t domain_ops = {
        .open = _task_domain_open,
        .lookup = _task_domain_lookup,
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
    if (!rsmgr_init_domain(RSRC_DOMAIN_TASK, "task", &domain_ops))
        return false;

    rsrc_t *root = rsmgr_new_resource(RSRC_DOMAIN_TASK, "/");
    if (root == NULL)
        return false;
    root->header.type = RSRC_TYPE_COLLECTION;
    root->ops = &_task_collection_ops;
    root->type_state = NULL;
    _task_domain_root_node = rsmgr_set_domain_root(RSRC_DOMAIN_TASK, root);
    if (_task_domain_root_node == NULL)
        return false;

    _task_domain_initialized = true;
    return true;
}

void task_domain_register(task_t *task)
{
    if (task == NULL || !_task_domain_initialized || _task_domain_root_node == NULL)
        return;
    if (!task->user_mode)
        return;
    if (task->resource_node != NULL)
        return;

    char name_buf[32];
    char *p = name_buf + sizeof(name_buf) - 1;
    *p = '\0';
    uint64_t id = task->id;
    if (id == 0) {
        *(--p) = '0';
    } else {
        while (id > 0) {
            *(--p) = '0' + (char) (id % 10);
            id /= 10;
        }
    }

    rsrc_t *resource = rsmgr_new_resource(RSRC_DOMAIN_TASK, p);
    if (resource == NULL)
        return;

    resource->header.type = RSRC_TYPE_COLLECTION;
    resource->ops = &_task_collection_ops;
    resource->type_state = task;

    rsrc_node_t *parent_node = _task_domain_root_node;
    if (task->parent != NULL && task->parent->resource_node != NULL)
        parent_node = task->parent->resource_node;

    rsrc_node_t *node = rsmgr_attach_resource(parent_node, resource);
    if (node == NULL) {
        kfree(resource);
        return;
    }

    task->resource_node = node;
}

void task_domain_unregister(task_t *task)
{
    if (task == NULL || task->resource_node == NULL)
        return;

    rsrc_node_t *node = task->resource_node;
    task->resource_node = NULL;

    while (node->first_child != NULL) {
        rsrc_node_t *child = node->first_child;
        task_t *child_task = (task_t *) child->resource->type_state;
        if (child_task != NULL)
            child_task->resource_node = NULL;
        _task_domain_detach_node(child);
        kfree(child->resource);
        kfree(child);
    }

    _task_domain_detach_node(node);
    kfree(node->resource);
    kfree(node);
}

void task_domain_reparent(task_t *task)
{
    if (task == NULL || task->resource_node == NULL || !_task_domain_initialized)
        return;

    rsrc_node_t *new_parent = _task_domain_root_node;
    if (task->parent != NULL && task->parent->resource_node != NULL)
        new_parent = task->parent->resource_node;

    _task_domain_detach_node(task->resource_node);

    task->resource_node->parent = new_parent;
    task->resource_node->next_sibling = NULL;

    if (new_parent->first_child == NULL) {
        new_parent->first_child = task->resource_node;
    } else {
        rsrc_node_t *last = new_parent->first_child;
        while (last->next_sibling != NULL)
            last = last->next_sibling;
        last->next_sibling = task->resource_node;
    }
}
