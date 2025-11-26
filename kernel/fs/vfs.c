/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/fs/vfs.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <stdint.h>

#define MAX_DRIVE_COUNT 16

static uint8_t _drives_count = 0;
static vfs_drive_t *_drives_map[MAX_DRIVE_COUNT] = {0};

vfs_drive_t *vfs_new_drive(const char *name)
{
    if (_drives_count >= MAX_DRIVE_COUNT)
        return NULL;

    /* Assign a unique drive name by appending a numeric suffix */
    size_t base_len = strlen(name);
    char final_name[40];
    for (int suffix = 0; suffix < MAX_DRIVE_COUNT; ++suffix) {
        /* Convert suffix to string */
        char suffix_str[4];
        int suffix_len = 0;
        int num = suffix;
        if (num == 0) {
            suffix_str[suffix_len++] = '0';
        } else {
            char rev[4];
            int rev_len = 0;
            while (num > 0 && rev_len < (int) sizeof(rev)) {
                rev[rev_len++] = '0' + (num % 10);
                num /= 10;
            }
            for (int j = rev_len - 1; j >= 0; --j)
                suffix_str[suffix_len++] = rev[j];
        }
        suffix_str[suffix_len] = '\0';
        /* Compose candidate name */
        if (base_len + suffix_len >= sizeof(final_name))
            continue;
        memcpy(final_name, name, base_len);
        memcpy(final_name + base_len, suffix_str, suffix_len + 1);
        /* Check uniqueness */
        bool exists = false;
        for (int i = 0; i < MAX_DRIVE_COUNT; ++i) {
            if (_drives_map[i] && strcmp(_drives_map[i]->name, final_name) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists)
            break;
    }

    uint8_t index;
    for (index = 0; index < MAX_DRIVE_COUNT; index++) {
        if (_drives_map[index] == NULL)
            break;
    }
    vfs_drive_t *drive = kmalloc(sizeof(vfs_drive_t));
    if (!drive)
        return NULL;
    drive->id = index;

    /* Copy the unique name we constructed */
    strncpy(drive->name, final_name, sizeof(drive->name) - 1);
    drive->name[sizeof(drive->name) - 1] = '\0';
    _drives_map[index] = drive;
    _drives_count++;
    return drive;
}

void vfs_remove_drive(vfs_drive_t *drive)
{
    _drives_map[drive->id] = NULL;
    _drives_count--;
    kfree(drive);
}

static vfs_drive_t *_vfs_get_drive_by_name(const char *name)
{
    for (int i = 0; i < MAX_DRIVE_COUNT; i++) {
        if (_drives_map[i] != NULL && strcmp(_drives_map[i]->name, name) == 0)
            return _drives_map[i];
    }
    return NULL;
}

void vfs_remove_drive_by_name(const char *name)
{
    vfs_drive_t *drive = _vfs_get_drive_by_name(name);
    if (drive)
        vfs_remove_drive(drive);
}

void vfs_add_child(vfs_node_t *parent, vfs_node_t *child)
{
    if (parent->child == NULL) {
        parent->child = child;
        child->sibling = NULL;
    } else {
        vfs_node_t *current = parent->child;
        while (current->sibling != NULL)
            current = current->sibling;
        current->sibling = child;
    }
    child->parent = parent;
}

void vfs_remove_child(vfs_node_t *parent, vfs_node_t *child)
{
    vfs_node_t *current = parent->child;
    vfs_node_t *prev = NULL;

    while (current) {
        if (current == child) {
            if (prev)
                prev->sibling = current->sibling;
            else
                parent->child = current->sibling;
            kfree(current);
            return;
        }
        prev = current;
        current = current->sibling;
    }
}

vfs_node_t *vfs_get_relative_path(vfs_node_t *base, const char *path)
{
    if (base == NULL)
        return NULL;

    /* Skip leading slashes */
    while (*path == '/')
        path++;

    /* If at end of path, return base node */
    if (*path == '\0') {
        return base;
    }

    vfs_node_t *current = base;

    while (*path) {
        const char *end = path;
        while (*end != '\0' && *end != '/') {
            end++;
        }

        size_t token_len = end - path;

        /* Skip empty tokens (caused by consecutive slashes) */
        if (token_len == 0) {
            path = end;
            while (*path == '/') {
                path++;
            }
            continue;
        }

        /* Current must be a directory to have children */
        if (current->type != DIRECTORY) {
            return NULL;
        }

        /* Search children for matching name */
        vfs_node_t *child = current->child;
        vfs_node_t *found = NULL;
        while (child != NULL) {
            if (strlen(child->name) == token_len && memcmp(child->name, path, token_len) == 0) {
                found = child;
                break;
            }
            child = child->sibling;
        }

        if (found == NULL)
            return NULL;

        current = found;
        path = end;

        /* Skip any slashes to move to next token */
        while (*path == '/')
            path++;
    }

    return current;
}

/*
 * Parse the full path into drive and path components.
 */
static int _get_path(const char *full_path, vfs_drive_t **drive, char *path, size_t buffer_size)
{
    if (full_path == NULL || *full_path == '\0')
        return -1;

    /* Check if this is a full path */
    const char *colon_pos = strchr(full_path, ':');
    if (colon_pos == NULL || colon_pos == full_path || *(colon_pos + 1) != '/')
        return -1; // Only full paths are supported

    /* Extract drive name from the path */
    size_t name_len = colon_pos - full_path;
    if (name_len >= sizeof((*drive)->name))
        return -1;

    char drive_name[sizeof((*drive)->name)];
    strncpy(drive_name, full_path, name_len);
    drive_name[name_len] = '\0';

    *drive = _vfs_get_drive_by_name(drive_name);
    if (*drive == NULL)
        return -1;

    /* Skip drive name part (name:/) */
    full_path = colon_pos + 2;

    char temp[buffer_size];
    size_t pos = 0;

    /* Process each component of the path */
    char *component_start = (char *) full_path;
    char *component_end;
    while (*component_start) {
        while (*component_start == '/')
            component_start++;
        if (!*component_start)
            break;

        /* Find end of component */
        component_end = component_start;
        while (*component_end != '\0' && *component_end != '/')
            component_end++;

        size_t component_len = component_end - component_start;

        /* Handle special components */
        if (component_len == 2 && component_start[0] == '.' && component_start[1] == '.') {
            /* ".." - go up one directory */
            if (pos > 1) { // Make sure we don't go past root
                pos--;     // Remove trailing slash
                while (pos > 1 && temp[pos - 1] != '/')
                    pos--;
                temp[pos] = '\0';
            }
        } else {
            /* Regular component, append it */
            if (pos + component_len + 1 >= buffer_size)
                return -1; // Buffer too small

            if (pos > 1 || temp[0] != '/') // Don't add slash if we're at root
                temp[pos++] = '/';
            strncpy(&temp[pos], component_start, component_len);
            pos += component_len;
            temp[pos] = '\0';
        }

        /* Move to next component */
        component_start = component_end;
    }

    /* If we ended up with an empty path, make sure it's at least "/" */
    if (pos == 0) {
        temp[pos++] = '/';
        temp[pos] = '\0';
    }

    /* Copy result to output buffer */
    if (strlen(temp) >= buffer_size)
        return -1;

    strcpy(path, temp);

    return 0;
}

file_t file_open(const char *full_path)
{
    vfs_drive_t *drive = NULL;
    char path[PATH_MAX];
    if (_get_path(full_path, &drive, path, sizeof(path)) < 0)
        return (file_t) {0};
    return drive->open((struct vfs_drive *) drive, path);
}

int file_create(const char *full_path, file_type_t type)
{
    vfs_drive_t *drive = NULL;
    char path[PATH_MAX];
    if (_get_path(full_path, &drive, path, sizeof(path)) < 0)
        return -1;
    return drive->create((struct vfs_drive *) drive, path, type);
}

int file_remove(const char *full_path)
{
    vfs_drive_t *drive = NULL;
    char path[PATH_MAX];
    if (_get_path(full_path, &drive, path, sizeof(path)) < 0)
        return -1;
    return drive->remove((struct vfs_drive *) drive, path);
}

int file_read(file_t *file, void *buffer, uint32_t size)
{
    return (file->drive)->read(file, buffer, size);
}

int file_write(file_t *file, const void *buffer, uint32_t size)
{
    return file->drive->write(file, buffer, size);
}

int file_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    return file->drive->seek(file, offset, mode);
}

int file_getdents(file_t *file, void *buffer, uint32_t size)
{
    return file->drive->getdents(file, buffer, size);
}

int file_getstats(file_t *file, file_stats_t *stats)
{
    return file->drive->getstats(file, stats);
}

size_t file_tell(file_t *file)
{
    return file->drive->tell(file);
}

int vfs_getdrives(void *buffer, uint32_t size)
{
    char *buf = (char *) buffer;
    uint32_t offset = 0;

    for (int i = 0; i < MAX_DRIVE_COUNT; i++) {
        vfs_drive_t *drive = _drives_map[i];
        if (drive == NULL)
            continue;

        size_t name_len = strlen(drive->name);
        uint32_t entry_len = sizeof(dir_entry_t) + name_len + 1;

        if (offset + entry_len > size)
            break;

        dir_entry_t *entry = (dir_entry_t *) (buf + offset);
        entry->length = entry_len;
        entry->type = DIRECTORY;
        memcpy(entry->name, drive->name, name_len + 1);

        offset += entry_len;
    }

    return offset;
}
