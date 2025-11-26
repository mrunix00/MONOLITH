/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/fs/vfs.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    size_t size;
    void *data;
} _tmpfs_inode_t;

static vfs_node_t *_new_tmpfs_node(file_type_t type)
{
    vfs_node_t *new_node = kmalloc(sizeof(vfs_node_t));
    if (new_node == NULL)
        return NULL;
    new_node->name = NULL;
    new_node->type = type;
    new_node->internal = kmalloc(sizeof(_tmpfs_inode_t));
    if (new_node->internal == NULL) {
        kfree(new_node);
        return NULL;
    }
    _tmpfs_inode_t *inode = new_node->internal;
    inode->data = NULL;
    inode->size = 0;
    return new_node;
}

static int _tmpfs_create(struct vfs_drive *drive, const char *name, file_type_t type)
{
    if (strcmp(name, "/") == 0)
        return -1;

    /* Find last slash to separate parent path and child name */
    const char *last_slash = strrchr(name, '/');
    const char *child_name;
    char *parent_path = NULL;

    if (last_slash == NULL) {
        /* No slash: parent is root, child name is whole string */
        child_name = name;
        parent_path = strdup("");
        if (parent_path == NULL)
            return -1;
    } else {
        child_name = last_slash + 1;
        /* Check if child name is empty (path ends with slash) */
        if (*child_name == '\0')
            return -1;

        size_t parent_len = last_slash - name;
        if (parent_len == 0) {
            /* Parent is root (path like "/file") */
            parent_path = strdup("/");
            if (parent_path == NULL)
                return -1;
        } else {
            parent_path = kmalloc(parent_len + 1);
            if (parent_path == NULL)
                return -1;
            memcpy(parent_path, name, parent_len);
            parent_path[parent_len] = '\0';
        }
    }

    vfs_node_t *parent = vfs_get_relative_path(((vfs_drive_t *) drive)->internal, parent_path);
    kfree(parent_path);

    if (!parent || parent->type != DIRECTORY) {
        return -1;
    }

    vfs_node_t *new_node = _new_tmpfs_node(type);
    if (!new_node)
        return -1;

    new_node->name = strdup(child_name);
    if (new_node->name == NULL) {
        kfree(new_node->internal);
        kfree(new_node);
        return -1;
    }

    new_node->child = NULL;
    vfs_add_child(parent, new_node);

    return 0;
}

static int _tmpfs_getdents(file_t *file, void *buffer, uint32_t size)
{
    if (((vfs_node_t *) file->internal)->type != DIRECTORY)
        return -1;

    vfs_node_t *current_file = ((vfs_node_t *) file->internal)->child;
    uint32_t buffer_offset = 0;
    size_t current_offset = 0;

    /* Skip entries that are entirely before the offset */
    while (current_file != NULL) {
        size_t name_len = strlen(current_file->name) + 1;
        size_t entry_size = sizeof(dir_entry_t) + name_len;

        if (current_offset + entry_size <= file->offset) {
            current_offset += entry_size;
            current_file = current_file->sibling;
        } else {
            break;
        }
    }

    /* Skip partially read entries (offset falls in the middle) */
    if (current_file != NULL) {
        size_t name_len = strlen(current_file->name) + 1;
        size_t entry_size = sizeof(dir_entry_t) + name_len;

        if (current_offset < file->offset) {
            current_offset += entry_size;
            current_file = current_file->sibling;
        }
    }

    /* Read directory entries */
    while (current_file != NULL) {
        size_t name_len = strlen(current_file->name) + 1;
        size_t entry_size = sizeof(dir_entry_t) + name_len;

        if (buffer_offset + entry_size > size) {
            break;
        }

        dir_entry_t *entry = (dir_entry_t *) ((char *) buffer + buffer_offset);
        entry->length = entry_size;
        entry->type = current_file->type;
        memcpy(entry->name, current_file->name, name_len);

        buffer_offset += entry_size;
        current_offset += entry_size;
        current_file = current_file->sibling;
    }

    /* Update the offset for the next read */
    file->offset = current_offset;

    return buffer_offset;
}

static int _tmpfs_remove(struct vfs_drive *drive, const char *name)
{
    vfs_node_t *file = vfs_get_relative_path(((vfs_drive_t *) drive)->internal, name);
    if (!file)
        return -1;

    kfree(((_tmpfs_inode_t *) file->internal)->data);
    kfree(file->internal);
    kfree(file->name);
    kfree(file);
    vfs_remove_child(file->parent, file);

    return 0;
}

static file_t _tmpfs_open(struct vfs_drive *drive, const char *path)
{
    vfs_node_t *vnode = vfs_get_relative_path(((vfs_drive_t *) drive)->internal, path);
    if (!vnode)
        return (file_t) {0};

    return (file_t) {
        .drive = drive,
        .internal = vnode,
        .offset = 0,
    };
}

static int _tmpfs_write(file_t *file, const void *buffer, uint32_t size)
{
    _tmpfs_inode_t *inode = ((vfs_node_t *) file->internal)->internal;
    if (inode->size < file->offset + size) {
        void *data = inode->data == NULL ? kmalloc(file->offset + size)
                                         : krealloc(inode->data, file->offset + size);
        if (data == NULL)
            return -1;
        inode->data = data;
        inode->size = file->offset + size;
    }

    memcpy(inode->data + file->offset, buffer, size);
    file->offset += size;
    return size;
}

static int _tmpfs_read(file_t *file, void *buffer, uint32_t size)
{
    vfs_node_t *vnode = file->internal;
    _tmpfs_inode_t *inode = vnode->internal;

    if (file->offset >= inode->size || inode->data == NULL)
        return 0;

    if (file->offset + size > inode->size)
        size = inode->size - file->offset;

    memcpy(buffer, inode->data + file->offset, size);
    file->offset += size;
    return size;
}

static int _tmpfs_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    vfs_node_t *vnode = file->internal;
    _tmpfs_inode_t *inode = vnode->internal;

    switch (mode) {
    case SEEK_SET:
        file->offset = offset;
        break;
    case SEEK_CUR:
        file->offset += offset;
        break;
    case SEEK_END:
        file->offset = inode->size + offset;
        break;
    }
    return 0;
}

int _tmpfs_getstats(file_t *file, file_stats_t *stats)
{
    if (file->internal == NULL)
        return -1;

    vfs_node_t *vnode = file->internal;
    _tmpfs_inode_t *inode = vnode->internal;

    *stats = (file_stats_t) {
        .size = inode->size,
        .type = vnode->type,
    };

    return 0;
}

static size_t _tmpfs_tell(file_t *file)
{
    if (file->internal == NULL)
        return -1;

    return file->offset;
}

vfs_drive_t *tmpfs_new_drive(const char *name)
{
    vfs_drive_t *drive = vfs_new_drive(name);
    if (drive == NULL)
        return NULL;

    drive->internal = _new_tmpfs_node(DIRECTORY);
    if (!drive->internal)
        goto failure;

    drive->create = _tmpfs_create;
    drive->remove = _tmpfs_remove;
    drive->open = _tmpfs_open;
    drive->write = _tmpfs_write;
    drive->read = _tmpfs_read;
    drive->seek = _tmpfs_seek;
    drive->tell = _tmpfs_tell;
    drive->getdents = _tmpfs_getdents;
    drive->getstats = _tmpfs_getstats;

    return drive;

failure:
    vfs_remove_drive(drive);
    return NULL;
}
