/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/fs/file_domain.h>
#include <kernel/rsmgr/rsmgr.h>

static rsrc_status_t _file_domain_open(
    rsrc_domain_t *domain, const char *path, rsrc_t **out_resource, void **out_handle_state)
{
    if (domain == NULL || path == NULL || out_resource == NULL || out_handle_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (domain->root_node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    if (path == NULL || (path[0] == '/' && path[1] == '\0')) {
        *out_resource = domain->root_node->resource;
        *out_handle_state = NULL;
        return RSRC_STATUS_OK;
    }

    rsrc_node_t *node = rsmgr_get_relative_path(domain->root_node, path);
    if (node == NULL)
        return RSRC_ERROR_NOT_FOUND;

    *out_resource = node->resource;
    *out_handle_state = NULL;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _file_domain_lookup(
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

bool file_domain_init()
{
    static bool initialized = false;

    if (initialized)
        return true;

    if (!rsmgr_init())
        return false;

    rsrc_ops_t ops = {
        .open = _file_domain_open,
        .lookup = _file_domain_lookup,
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

    if (!rsmgr_init_domain(RSRC_DOMAIN_FILE, "file", &ops))
        return false;

    initialized = true;
    return true;
}
