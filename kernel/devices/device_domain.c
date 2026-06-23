/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/device_domain.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>

static bool _device_initialized = false;

static int _list_collection_entries(
    rsrc_t *resource, uint64_t cursor, void *buffer, uint64_t buffer_len)
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

static rsrc_status_t _device_domain_open(
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

static rsrc_status_t _device_domain_lookup(
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

static rsrc_status_t _device_collection_list_op(
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

    int result = _list_collection_entries(resource, cursor, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_IO;

    *out_bytes_written = (uint64_t) result;
    if (out_next_cursor != NULL)
        *out_next_cursor = cursor + (uint64_t) result;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _device_collection_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = NULL,
    .list = _device_collection_list_op,
    .read = NULL,
    .write = NULL,
    .mmap = NULL,
    .poll = NULL,
    .remove = NULL,
    .control = NULL,
};

static rsrc_node_t *_get_device_root(void)
{
    rsrc_domain_t *domain = rsmgr_get_domain("device");
    return domain == NULL ? NULL : domain->root_node;
}

static rsrc_node_t *_ensure_collection_path(const char *path)
{
    if (path == NULL || path[0] != '/')
        return NULL;

    rsrc_node_t *current = _get_device_root();
    if (current == NULL)
        return NULL;

    const char *component = path;
    while (*component == '/')
        component++;

    if (*component == '\0')
        return current;

    while (*component != '\0') {
        const char *end = component;
        while (*end != '\0' && *end != '/')
            end++;

        size_t component_len = (size_t) (end - component);
        if (component_len == 0)
            return NULL;

        char name[RSRC_NAME_MAX_LEN];
        if (component_len >= sizeof(name))
            return NULL;
        memcpy(name, component, component_len);
        name[component_len] = '\0';

        rsrc_node_t *child = rsmgr_get_relative_path(current, name);
        if (child == NULL) {
            rsrc_t *resource = rsmgr_new_resource(RSRC_DOMAIN_DEVICE, name);
            if (resource == NULL)
                return NULL;

            resource->header.type = RSRC_TYPE_COLLECTION;
            resource->ops = &_device_collection_ops;
            resource->type_state = NULL;
            child = rsmgr_attach_resource(current, resource);
            if (child == NULL) {
                kfree(resource);
                return NULL;
            }
        } else if (child->resource->header.type != RSRC_TYPE_COLLECTION) {
            return NULL;
        }

        current = child;
        component = end;
        while (*component == '/')
            component++;
    }

    return current;
}

bool device_domain_init(void)
{
    if (_device_initialized)
        return true;
    if (!rsmgr_init())
        return false;

    rsrc_ops_t domain_ops = {
        .open = _device_domain_open,
        .lookup = _device_domain_lookup,
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
    if (!rsmgr_init_domain(RSRC_DOMAIN_DEVICE, "device", &domain_ops))
        return false;

    rsrc_t *root = rsmgr_new_resource(RSRC_DOMAIN_DEVICE, "/");
    if (root == NULL)
        return false;

    root->header.type = RSRC_TYPE_COLLECTION;
    root->ops = &_device_collection_ops;
    root->type_state = NULL;
    if (rsmgr_set_domain_root(RSRC_DOMAIN_DEVICE, root) == NULL)
        return false;

    _device_initialized = true;
    return true;
}

rsrc_node_t *device_register(const char *path, const rsrc_ops_t *ops, void *type_state)
{
    if (!device_domain_init() || path == NULL || path[0] != '/')
        return NULL;

    if (strcmp(path, "/") == 0)
        return _get_device_root();

    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
        return NULL;

    char parent_path[RSRC_PATH_MAX_LEN];
    size_t parent_len = (size_t) (last_slash - path);
    if (parent_len == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        if (parent_len >= sizeof(parent_path))
            return NULL;
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }

    const char *name = last_slash + 1;
    if (*name == '\0' || strlen(name) >= RSRC_NAME_MAX_LEN)
        return NULL;

    rsrc_node_t *parent = _ensure_collection_path(parent_path);
    if (parent == NULL)
        return NULL;

    rsrc_node_t *existing = rsmgr_get_relative_path(parent, name);
    if (existing != NULL) {
        bool is_collection = ops == NULL;
        bool existing_is_collection = existing->resource->header.type == RSRC_TYPE_COLLECTION;
        return is_collection == existing_is_collection ? existing : NULL;
    }

    bool is_collection = (ops == NULL);
    rsrc_type_t type = is_collection ? RSRC_TYPE_COLLECTION : RSRC_TYPE_RESOURCE;

    rsrc_t *resource = rsmgr_new_resource(RSRC_DOMAIN_DEVICE, name);
    if (resource == NULL)
        return NULL;

    resource->header.type = type;
    resource->ops = is_collection ? &_device_collection_ops : ops;
    resource->type_state = is_collection ? NULL : type_state;

    rsrc_node_t *node = rsmgr_attach_resource(parent, resource);
    if (node == NULL) {
        kfree(resource);
        return NULL;
    }

    return node;
}
