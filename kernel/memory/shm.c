/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/shm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/syscall.h>
#include <shared/include/monolith/stdio.h>

typedef struct
{
    uintptr_t phys_addr;
    size_t size;
    size_t page_count;
} _shm_object_t;

static bool _shm_initialized = false;
static const rsrc_ops_t _shm_ops;

static rsrc_status_t _shm_domain_open(
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

static rsrc_status_t _shm_domain_lookup(
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

static rsrc_status_t _shm_list_op(
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

static rsrc_status_t _shm_read_op(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_read == NULL
        || resource->header.type != RSRC_TYPE_RESOURCE || resource->type_state == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    uint64_t offset = *(uint64_t *) handle_state;
    if (offset >= object->size)
        return (*out_bytes_read = 0), RSRC_STATUS_OK;

    if (offset + buffer_len > object->size)
        buffer_len = object->size - offset;

    memcpy(buffer, (uint8_t *) vmm_get_hhdm_addr((void *) object->phys_addr) + offset, buffer_len);
    *out_bytes_read = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _shm_write_op(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_written == NULL
        || resource->header.type != RSRC_TYPE_RESOURCE || resource->type_state == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    uint64_t offset = *(uint64_t *) handle_state;
    if (offset >= object->size)
        return RSRC_ERROR_OUT_OF_RANGE;

    if (offset + buffer_len > object->size)
        buffer_len = object->size - offset;

    memcpy((uint8_t *) vmm_get_hhdm_addr((void *) object->phys_addr) + offset, buffer, buffer_len);
    *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _shm_seek_op(
    rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset)
{
    if (resource == NULL || handle_state == NULL || out_offset == NULL
        || resource->type_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    int64_t base = 0;
    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (int64_t) *(uint64_t *) handle_state;
        break;
    case SEEK_END:
        base = (int64_t) object->size;
        break;
    default:
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    int64_t target = base + offset;
    if (target < 0)
        target = 0;
    *out_offset = (uint64_t) target;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _shm_mmap_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t offset,
    uint64_t length,
    uint64_t prot,
    uint64_t *out_address)
{
    if (resource == NULL || out_address == NULL || resource->type_state == NULL || length == 0
        || handle_state == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    if (offset != 0 || length > object->size)
        return RSRC_ERROR_OUT_OF_RANGE;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    uintptr_t virt_addr = task_find_free_vaddr(current, object->page_count);
    if (virt_addr == 0)
        return RSRC_ERROR_NO_MEMORY;

    uint64_t pt_flags = PTFLAG_P | PTFLAG_US;
    if (prot & ALLOC_PAGES_FLAG_RW)
        pt_flags |= PTFLAG_RW;
    if (!(prot & ALLOC_PAGES_FLAG_EXEC))
        pt_flags |= PTFLAG_XD;

    if (task_map(current, virt_addr, object->phys_addr, object->page_count, pt_flags, false) < 0)
        return RSRC_ERROR_NO_MEMORY;

    *out_address = virt_addr;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _shm_describe_op(rsrc_t *resource, rsrc_info_t *out_info)
{
    if (resource == NULL || out_info == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (resource->type_state == NULL) {
        out_info->shm.size = 0;
        out_info->shm.page_count = 0;
        return RSRC_STATUS_OK;
    }

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    out_info->shm.size = (uint64_t) object->size;
    out_info->shm.page_count = (uint64_t) object->page_count;
    return RSRC_STATUS_OK;
}

static void _shm_destroy_op(rsrc_t *resource)
{
    if (resource == NULL || resource->type_state == NULL)
        return;

    _shm_object_t *object = (_shm_object_t *) resource->type_state;
    if (object->phys_addr != 0 && object->page_count > 0)
        pmm_free((void *) object->phys_addr, object->page_count);
    kfree(object);
    resource->type_state = NULL;
}

rsrc_status_t shm_create(const char *name, size_t size, rsrc_t **out_resource)
{
    if (size == 0 || out_resource == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    size_t page_count = PAGE_UP(size) / PAGE_SIZE;
    if (page_count == 0)
        return RSRC_ERROR_INVALID_ARGUMENT;

    void *phys_mem = pmm_alloc(page_count);
    if (phys_mem == NULL)
        return RSRC_ERROR_NO_MEMORY;

    memset(vmm_get_hhdm_addr(phys_mem), 0, page_count * PAGE_SIZE);

    rsrc_t *shm = rsmgr_new_resource(RSRC_DOMAIN_SHM, "anon");
    if (shm == NULL) {
        pmm_free(phys_mem, page_count);
        return RSRC_ERROR_NO_MEMORY;
    }

    _shm_object_t *object = (_shm_object_t *) kmalloc(sizeof(_shm_object_t));
    if (object == NULL) {
        pmm_free(phys_mem, page_count);
        kfree(shm);
        return RSRC_ERROR_NO_MEMORY;
    }

    memset(object, 0, sizeof(*object));
    object->phys_addr = (uintptr_t) phys_mem;
    object->size = size;
    object->page_count = page_count;

    shm->header.type = RSRC_TYPE_RESOURCE;
    shm->ops = &_shm_ops;
    shm->type_state = object;

    if (name != NULL) {
        rsrc_status_t result
            = rsmgr_attach_resource_at_path(RSRC_DOMAIN_SHM, name, shm, &_shm_ops);
        if (result != RSRC_STATUS_OK) {
            pmm_free(phys_mem, page_count);
            kfree(object);
            kfree(shm);
            return result;
        }
    }

    *out_resource = shm;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _shm_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = _shm_destroy_op,
    .describe = _shm_describe_op,
    .seek = _shm_seek_op,
    .list = _shm_list_op,
    .read = _shm_read_op,
    .write = _shm_write_op,
    .mmap = _shm_mmap_op,
    .poll = NULL,
    .remove = NULL,
    .control = NULL,
};

bool shm_domain_init(void)
{
    if (_shm_initialized)
        return true;

    if (!rsmgr_init())
        return false;

    rsrc_ops_t domain_ops = {
        .open = _shm_domain_open,
        .lookup = _shm_domain_lookup,
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
    if (!rsmgr_init_domain(RSRC_DOMAIN_SHM, "shm", &domain_ops))
        return false;

    rsrc_t *root = rsmgr_new_resource(RSRC_DOMAIN_SHM, "/");
    if (root == NULL)
        return false;

    root->header.type = RSRC_TYPE_COLLECTION;
    root->ops = &_shm_ops;
    root->type_state = NULL;
    if (rsmgr_set_domain_root(RSRC_DOMAIN_SHM, root) == NULL)
        return false;

    _shm_initialized = true;
    return true;
}
