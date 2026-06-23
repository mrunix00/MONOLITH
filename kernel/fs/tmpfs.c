/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/fs/file_domain.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/ustar.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <shared/include/monolith/stdio.h>

typedef struct
{
    size_t size;
    void *data;
} _tmpfs_inode_t;

static int _tmpfs_create_resource(rsrc_node_t *parent, const char *name, rsrc_type_t type);

static uint64_t _tmpfs_child_count(rsrc_t *resource)
{
    if (resource == NULL || resource->node == NULL)
        return 0;

    uint64_t count = 0;
    for (rsrc_node_t *child = resource->node->first_child; child != NULL;
         child = child->next_sibling) {
        count++;
    }
    return count;
}

static rsrc_status_t _tmpfs_describe_op(rsrc_t *resource, rsrc_info_t *out_info)
{
    if (resource == NULL || out_info == NULL || resource->type_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    _tmpfs_inode_t *inode = resource->type_state;
    out_info->file.size = (uint64_t) inode->size;
    out_info->file.child_count = _tmpfs_child_count(resource);
    return RSRC_STATUS_OK;
}

static rsrc_status_t _tmpfs_read_op(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_read == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (resource->header.type == RSRC_TYPE_COLLECTION)
        return RSRC_ERROR_NOT_SUPPORTED;

    int result = tmpfs_read(resource, *(uint64_t *) handle_state, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_IO;

    *out_bytes_read = result;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _tmpfs_write_op(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (resource->header.type == RSRC_TYPE_COLLECTION)
        return RSRC_ERROR_NOT_SUPPORTED;

    int result = tmpfs_write(resource, *(uint64_t *) handle_state, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_IO;

    *out_bytes_written = result;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _tmpfs_list_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written)
{
    (void) handle_state;

    if (resource == NULL || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (resource->header.type != RSRC_TYPE_COLLECTION)
        return RSRC_ERROR_NOT_SUPPORTED;

    int result = tmpfs_list(resource, cursor, buffer, buffer_len);
    if (result < 0)
        return RSRC_ERROR_IO;

    *out_bytes_written = result;
    if (out_next_cursor != NULL)
        *out_next_cursor = cursor + result;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _tmpfs_seek_op(
    rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset)
{
    if (resource == NULL || resource->type_state == NULL || handle_state == NULL
        || out_offset == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (resource->header.type == RSRC_TYPE_COLLECTION)
        return RSRC_ERROR_NOT_SUPPORTED;

    _tmpfs_inode_t *inode = resource->type_state;
    int64_t base = 0;

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (int64_t) *(uint64_t *) handle_state;
        break;
    case SEEK_END:
        base = (int64_t) inode->size;
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

static rsrc_status_t _tmpfs_create_op(
    rsrc_t *resource, void *handle_state, const char *name, rsrc_type_t type, rsrc_t **out_resource)
{
    (void) handle_state;

    if (resource == NULL || resource->header.type != RSRC_TYPE_COLLECTION || resource->node == NULL
        || name == NULL || out_resource == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (type != RSRC_TYPE_COLLECTION && type != RSRC_TYPE_RESOURCE)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (_tmpfs_create_resource(resource->node, name, type) < 0)
        return RSRC_ERROR_NO_MEMORY;

    rsrc_node_t *node = rsmgr_get_relative_path(resource->node, name);
    if (node == NULL || node->resource == NULL)
        return RSRC_ERROR_NOT_FOUND;

    *out_resource = node->resource;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _tmpfs_resource_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = _tmpfs_describe_op,
    .seek = _tmpfs_seek_op,
    .list = _tmpfs_list_op,
    .read = _tmpfs_read_op,
    .write = _tmpfs_write_op,
    .mmap = NULL,
    .poll = NULL,
    .create = _tmpfs_create_op,
    .remove = NULL,
    .control = NULL,
};

rsrc_t *tmpfs_new_resource(const char *name, rsrc_type_t type)
{
    rsrc_t *resource = rsmgr_new_resource(RSRC_DOMAIN_FILE, name);
    if (resource == NULL)
        return NULL;

    resource->header.type = type;
    resource->ops = &_tmpfs_resource_ops;

    _tmpfs_inode_t *inode = kmalloc(sizeof(_tmpfs_inode_t));
    if (inode == NULL) {
        kfree(resource);
        return NULL;
    }
    inode->data = NULL;
    inode->size = 0;
    resource->type_state = inode;

    return resource;
}

static int _tmpfs_create_resource(rsrc_node_t *parent, const char *name, rsrc_type_t type)
{
    if (parent == NULL || name == NULL)
        return -1;

    if (strcmp(name, "/") == 0)
        return -1;

    /* Find last slash to separate parent path and child name */
    const char *last_slash = strrchr(name, '/');
    const char *child_name;
    char parent_path[RSRC_NAME_MAX_LEN];

    if (last_slash == NULL) {
        /* No slash: parent is root, child name is whole string */
        child_name = name;
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        child_name = last_slash + 1;
        if (*child_name == '\0')
            return -1;

        size_t parent_len = last_slash - name;
        if (parent_len == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            if (parent_len >= RSRC_NAME_MAX_LEN)
                return -1;
            memcpy(parent_path, name, parent_len);
            parent_path[parent_len] = '\0';
        }
    }

    /* Find parent node */
    rsrc_node_t *parent_node = rsmgr_get_relative_path(parent, parent_path);
    if (parent_node == NULL || parent_node->resource->header.type != RSRC_TYPE_COLLECTION)
        return -1;

    if (rsmgr_get_relative_path(parent_node, child_name) != NULL)
        return 0;

    rsrc_t *resource = tmpfs_new_resource(child_name, type);
    if (resource == NULL)
        return -1;

    /* Attach to tree */
    rsrc_node_t *node = rsmgr_attach_resource(parent_node, resource);
    if (node == NULL) {
        kfree(resource->type_state);
        kfree(resource);
        return -1;
    }

    return 0;
}

int tmpfs_create(rsrc_node_t *root, const char *path, rsrc_type_t type)
{
    return _tmpfs_create_resource(root, path, type);
}

int tmpfs_read(rsrc_t *resource, uint64_t offset, void *buffer, uint64_t size)
{
    if (resource == NULL || resource->type_state == NULL || buffer == NULL)
        return -1;

    _tmpfs_inode_t *inode = resource->type_state;

    if (offset >= inode->size || inode->data == NULL)
        return 0;

    if (offset + size > inode->size)
        size = inode->size - offset;

    memcpy(buffer, (uint8_t *) inode->data + offset, size);
    return size;
}

int tmpfs_write(rsrc_t *resource, uint64_t offset, const void *buffer, uint64_t size)
{
    if (resource == NULL || resource->type_state == NULL || buffer == NULL)
        return -1;

    _tmpfs_inode_t *inode = resource->type_state;

    if (inode->size < offset + size) {
        void *data = inode->data == NULL ? kmalloc(offset + size)
                                         : krealloc(inode->data, offset + size);
        if (data == NULL)
            return -1;
        inode->data = data;
        inode->size = offset + size;
    }

    memcpy((uint8_t *) inode->data + offset, buffer, size);
    return size;
}

int tmpfs_list(rsrc_t *resource, uint64_t cursor, void *buffer, uint64_t buffer_len)
{
    if (resource == NULL || resource->header.type != RSRC_TYPE_COLLECTION)
        return -1;

    rsrc_node_t *target = resource->node;
    if (target == NULL)
        return -1;

    rsrc_node_t *current = target->first_child;
    uint32_t buffer_offset = 0;
    size_t current_offset = 0;

    /* Skip entries before cursor */
    while (current != NULL) {
        size_t name_len = strlen(current->resource->header.name);
        size_t entry_size = sizeof(uint32_t) + name_len + 1;

        if (current_offset + entry_size <= cursor) {
            current_offset += entry_size;
            current = current->next_sibling;
        } else {
            break;
        }
    }

    /* Read entries */
    while (current != NULL) {
        size_t name_len = strlen(current->resource->header.name);
        size_t entry_size = sizeof(uint32_t) + name_len + 1;

        if (buffer_offset + entry_size > buffer_len)
            break;

        /* Write record_size */
        uint32_t record_size = (uint32_t) entry_size;
        memcpy((uint8_t *) buffer + buffer_offset, &record_size, sizeof(uint32_t));
        buffer_offset += sizeof(uint32_t);

        /* Write name */
        memcpy((uint8_t *) buffer + buffer_offset, current->resource->header.name, name_len + 1);
        buffer_offset += name_len + 1;

        current_offset += entry_size;
        current = current->next_sibling;
    }

    return buffer_offset;
}

static int _oct2bin(char *str, int size)
{
    int n = 0;
    char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

static int _tmpfs_ensure_path_exists(rsrc_node_t *root, const char *path)
{
    if (path == NULL || *path == '\0' || strcmp(path, "/") == 0)
        return 0;

    const char *p = path;
    while (*p == '/')
        p++;

    if (*p == '\0')
        return 0;

    char *path_copy = strdup(p);
    if (path_copy == NULL)
        return -1;

    char *component = path_copy;
    char current_path[RSRC_NAME_MAX_LEN];
    current_path[0] = '/';
    current_path[1] = '\0';

    while (*component) {
        char *slash = strchr(component, '/');
        if (slash)
            *slash = '\0';

        /* Check if this component already exists */
        rsrc_node_t *node = rsmgr_get_relative_path(root, current_path);
        if (node == NULL || node->resource->header.type != RSRC_TYPE_COLLECTION) {
            /* Create the directory */
            if (_tmpfs_create_resource(root, current_path, RSRC_TYPE_COLLECTION) < 0) {
                kfree(path_copy);
                return -1;
            }
        }

        /* Append component to current path for next iteration */
        size_t len = strlen(current_path);
        if (len > 1) {
            if (len + 1 < RSRC_NAME_MAX_LEN) {
                current_path[len] = '/';
                current_path[len + 1] = '\0';
                len++;
            }
        }
        size_t comp_len = strlen(component);
        if (len + comp_len < RSRC_NAME_MAX_LEN) {
            strcat(current_path, component);
        }

        if (slash) {
            component = slash + 1;
        } else {
            break;
        }
    }

    kfree(path_copy);
    return 0;
}

int tmpfs_populate_from_initrd(rsrc_node_t *root, void *ustar_data)
{
    if (root == NULL || ustar_data == NULL)
        return -1;

    ustar_block_t *block = (ustar_block_t *) ustar_data;

    while (memcmp(block->magic, "ustar", 5) == 0) {
        /* Build full path */
        char full_path[RSRC_NAME_MAX_LEN] = {0};

        if (block->prefix[0] != '\0') {
            strncat(full_path, block->prefix, sizeof(full_path) - 1);
            strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
            strncat(full_path, block->filename, sizeof(full_path) - strlen(full_path) - 1);
        } else {
            strncpy(full_path, block->filename, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
        }

        /* Normalize path - remove leading "./" */
        if (strncmp(full_path, "./", 2) == 0) {
            char *rest = full_path + 2;
            memcpy(full_path, rest, strlen(rest) + 1);
        }

        /* Remove trailing slash for directories */
        size_t len = strlen(full_path);
        if (len > 0 && full_path[len - 1] == '/' && block->flag == USTAR_DIRECTORY) {
            full_path[len - 1] = '\0';
        }

        /* Skip root directory entry */
        if (strcmp(full_path, ".") == 0 || strcmp(full_path, "./") == 0
            || strcmp(full_path, "") == 0) {
            size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
            size_t total_size = 512 + ((file_size + 511) & ~511);
            block = (ustar_block_t *) ((char *) block + total_size);
            continue;
        }

        /* Ensure parent directories exist */
        const char *last_slash = strrchr(full_path, '/');
        if (last_slash != NULL) {
            size_t parent_len = last_slash - full_path;
            char parent_path[RSRC_NAME_MAX_LEN];
            if (parent_len >= RSRC_NAME_MAX_LEN) {
                size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
                size_t total_size = 512 + ((file_size + 511) & ~511);
                block = (ustar_block_t *) ((char *) block + total_size);
                continue;
            }
            memcpy(parent_path, full_path, parent_len);
            parent_path[parent_len] = '\0';
            _tmpfs_ensure_path_exists(root, parent_path);
        }

        /* Create file or directory */
        rsrc_type_t type = (block->flag == USTAR_DIRECTORY) ? RSRC_TYPE_COLLECTION
                                                            : RSRC_TYPE_RESOURCE;
        _tmpfs_create_resource(root, full_path, type);

        /* If it's a file, write its contents */
        if (block->flag != USTAR_DIRECTORY) {
            size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
            if (file_size > 0) {
                rsrc_node_t *file_node = rsmgr_get_relative_path(root, full_path);
                if (file_node != NULL) {
                    void *file_data = ((char *) block) + sizeof(ustar_block_t);
                    tmpfs_write(file_node->resource, 0, file_data, file_size);
                }
            }
        }

        /* Move to next block */
        size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
        size_t total_size = 512 + ((file_size + 511) & ~511);
        block = (ustar_block_t *) ((char *) block + total_size);
    }

    return 0;
}

rsrc_node_t *tmpfs_mount(const char *name)
{
    if (name == NULL || *name == '\0')
        return NULL;

    if (!file_domain_init())
        return NULL;

    rsrc_domain_t *domain = rsmgr_get_domain("file");
    if (domain == NULL)
        return NULL;

    if (domain->root_node == NULL) {
        rsrc_t *root_res = tmpfs_new_resource("/", RSRC_TYPE_COLLECTION);
        if (root_res == NULL)
            return NULL;

        if (rsmgr_set_domain_root(RSRC_DOMAIN_FILE, root_res) == NULL) {
            kfree(root_res->type_state);
            kfree(root_res);
            return NULL;
        }
    }

    rsrc_node_t *existing = rsmgr_get_relative_path(domain->root_node, name);
    if (existing != NULL)
        return existing;

    rsrc_t *mount_res = tmpfs_new_resource(name, RSRC_TYPE_COLLECTION);
    if (mount_res == NULL)
        return NULL;

    rsrc_node_t *mount_node = rsmgr_attach_resource(domain->root_node, mount_res);
    if (mount_node == NULL) {
        kfree(mount_res->type_state);
        kfree(mount_res);
        return NULL;
    }

    return mount_node;
}
