/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/debug.h>
#include <kernel/fs/initrdfs.h>
#include <kernel/fs/ustar.h>
#include <kernel/fs/vfs.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <stdint.h>

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

static ustar_block_t *_initrd_find_block(void *data, const char *path)
{
    ustar_block_t *block = data;

    /* Skip leading '/' and handle root */
    const char *p = path;
    while (*p == '/')
        p++;
    if (*p == '\0')
        p = ".";

    while (memcmp(block->magic, "ustar", 5) == 0) {
        char full_path[PATH_MAX] = {0};

        if (block->prefix[0] != '\0') {
            strncat(full_path, block->prefix, sizeof(full_path) - 1);
            strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
            strncat(full_path, block->filename, sizeof(full_path) - strlen(full_path) - 1);
        } else {
            strncpy(full_path, block->filename, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
        }

        /* Normalize root directory and file paths */
        if (strcmp(full_path, "./") == 0) {
            strcpy(full_path, ".");
        } else if (strncmp(full_path, "./", 2) == 0) {
            /* Remove leading "./" from paths */
            char *rest = full_path + 2;
            memcpy(full_path, rest, strlen(rest) + 1);
        }

        /* Remove trailing slash for directories */
        size_t len = strlen(full_path);
        if (len > 0 && full_path[len - 1] == '/' && block->flag == USTAR_DIRECTORY) {
            full_path[len - 1] = '\0';
        }

        /* Normalize given path by removing trailing slash */
        char normalized_path[PATH_MAX];
        strncpy(normalized_path, p, sizeof(normalized_path));
        normalized_path[sizeof(normalized_path) - 1] = '\0';
        len = strlen(normalized_path);
        if (len > 0 && normalized_path[len - 1] == '/') {
            normalized_path[len - 1] = '\0';
        }

        if (strcmp(full_path, normalized_path) == 0)
            return block;

        size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
        size_t total_size = 512 + ((file_size + 511) & ~511);
        block = (ustar_block_t *) ((char *) block + total_size);
    }
    return NULL;
}

static file_t _initrd_open(struct vfs_drive *drive, const char *path)
{
    ustar_block_t *block = _initrd_find_block(drive->internal, path);
    if (block == NULL)
        return (file_t) {0};

    return (file_t) {
        .drive = drive,
        .internal = block,
        .offset = 0,
    };
}

static int _initrd_create(struct vfs_drive *, const char *, file_type_t)
{
    return -1;
}

static int _initrd_remove(struct vfs_drive *, const char *)
{
    return -1;
}

static int _initrd_write(file_t *, const void *, uint32_t)
{
    return -1;
}

static int _initrd_read(file_t *file, void *buffer, uint32_t size)
{
    ustar_block_t *block = file->internal;
    size_t block_size = _oct2bin(block->size, sizeof(block->size) - 1);
    if (file->offset + size > block_size)
        size = block_size - file->offset;

    void *data = ((char *) block) + sizeof(ustar_block_t) + file->offset;
    memcpy(buffer, data, size);
    file->offset += size;

    return size;
}

static int _initrd_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    ustar_block_t *block = file->internal;
    size_t size = _oct2bin(block->size, sizeof(block->size) - 1);
    switch (mode) {
    case SEEK_SET:
        file->offset = offset;
        break;
    case SEEK_CUR:
        file->offset += offset;
        break;
    case SEEK_END:
        file->offset = size + offset;
        break;
    }
    return 0;
}

static int _initrd_getdents(file_t *file, void *buffer, uint32_t size)
{
    ustar_block_t *dir_block = file->internal;
    if (dir_block->flag != USTAR_DIRECTORY)
        return -1;

    char dir_path[PATH_MAX] = {0};
    size_t dir_path_len = 0;

    /* Handle root directory ("./" or ".") */
    int is_root = 0;
    if (dir_block->prefix[0] == '\0'
        && (strcmp(dir_block->filename, "./") == 0 || strcmp(dir_block->filename, ".") == 0)) {
        is_root = 1;
        /* Root directory - set path to empty */
        dir_path[0] = '\0';
        dir_path_len = 0;
    } else {
        /* Build directory path normally */
        if (dir_block->prefix[0] != '\0') {
            strncat(dir_path, dir_block->prefix, sizeof(dir_path) - 1);
            strncat(dir_path, "/", sizeof(dir_path) - strlen(dir_path) - 1);
            strncat(dir_path, dir_block->filename, sizeof(dir_path) - strlen(dir_path) - 1);
        } else {
            strncpy(dir_path, dir_block->filename, sizeof(dir_path) - 1);
        }
        dir_path[sizeof(dir_path) - 1] = '\0';

        /* Ensure directory path ends with '/' */
        size_t len = strlen(dir_path);
        if (len > 0 && dir_path[len - 1] != '/') {
            if (len + 1 < sizeof(dir_path)) {
                dir_path[len] = '/';
                dir_path[len + 1] = '\0';
                len++;
            }
        }
        dir_path_len = len;
    }

    void *base = dir_block;
    size_t current_offset = file->offset;

    /* Initialize offset to the block after the directory if starting */
    if (current_offset == 0) {
        current_offset = (char *) dir_block - (char *) base + 512;
    }

    char *bufptr = buffer;
    uint32_t remaining = size;
    int bytes_written = 0;

    while (remaining > 0) {
        ustar_block_t *block = (ustar_block_t *) ((char *) base + current_offset);

        /* Check for end of archive (zero block) */
        if (block->filename[0] == '\0') {
            break;
        }
        if (memcmp(block->magic, "ustar", 5) != 0) {
            break;
        }

        /* Skip zero-sized blocks (invalid) */
        size_t file_size = _oct2bin(block->size, sizeof(block->size) - 1);
        if (file_size == 0 && block->flag != USTAR_DIRECTORY) {
            current_offset += 512;
            continue;
        }

        char current_path[PATH_MAX] = {0};
        if (block->prefix[0] != '\0') {
            strncat(current_path, block->prefix, sizeof(current_path) - 1);
            strncat(current_path, "/", sizeof(current_path) - strlen(current_path) - 1);
            strncat(current_path, block->filename, sizeof(current_path) - strlen(current_path) - 1);
        } else {
            strncpy(current_path, block->filename, sizeof(current_path) - 1);
        }
        current_path[sizeof(current_path) - 1] = '\0';

        /* For root directory, show top-level entries */
        char *name_start = current_path;
        if (is_root) {
            /* Skip leading "./" for root directory entries */
            if (strncmp(current_path, "./", 2) == 0) {
                name_start += 2;
            }
        }

        /* Check if the current block is in the target directory */
        if (strncmp(current_path, dir_path, dir_path_len) == 0) {
            char *rest = name_start + dir_path_len;

            /* Skip if rest is empty (directory itself) */
            if (*rest == '\0') {
                current_offset += 512 + ((file_size + 511) & ~511);
                continue;
            }

            /* Skip if path contains subdirectories */
            char *first_slash = strchr(rest, '/');
            if (first_slash != NULL && first_slash[1] != '\0') {
                current_offset += 512 + ((file_size + 511) & ~511);
                continue;
            }

            /* Remove trailing slash from directory names */
            size_t name_len = strlen(rest);
            if (name_len > 0 && rest[name_len - 1] == '/') {
                rest[name_len - 1] = '\0';
                name_len--;
            }

            /* Skip if name is empty after processing */
            if (name_len == 0) {
                current_offset += 512 + ((file_size + 511) & ~511);
                continue;
            }

            size_t entry_length = sizeof(dir_entry_t) + name_len + 1;

            if (remaining < entry_length)
                break;

            dir_entry_t *entry = (dir_entry_t *) bufptr;
            entry->length = entry_length;
            entry->type = (block->flag == USTAR_DIRECTORY) ? DIRECTORY : FILE;
            strcpy(entry->name, rest);

            bufptr += entry_length;
            remaining -= entry_length;
            bytes_written += entry_length;
        }

        /* Move to next block */
        current_offset += 512 + ((file_size + 511) & ~511);
    }

    file->offset = current_offset;

    return bytes_written;
}

static int _initrd_getstats(file_t *file, file_stats_t *stats)
{
    ustar_block_t *block = file->internal;
    size_t size = _oct2bin(block->size, sizeof(block->size) - 1);
    stats->size = size;
    stats->type = block->flag == USTAR_DIRECTORY ? DIRECTORY : FILE;
    return 0;
}

vfs_drive_t *initrd_new_drive(const char *prefix, void *data)
{
    vfs_drive_t *new_drive = vfs_new_drive(prefix);
    if (new_drive == NULL)
        return NULL;

    new_drive->internal = data;
    new_drive->open = _initrd_open;
    new_drive->create = _initrd_create;
    new_drive->remove = _initrd_remove;
    new_drive->write = _initrd_write;
    new_drive->read = _initrd_read;
    new_drive->seek = _initrd_seek;
    new_drive->getdents = _initrd_getdents;
    new_drive->getstats = _initrd_getstats;

    return new_drive;
}

void initrd_load_modules(struct limine_module_response *response)
{
    debug_log("[*] Loading kernel modules...\n");
    for (uint64_t i = 0; i < response->module_count; i++) {
        struct limine_file *module = response->modules[i];
        if (initrd_new_drive("system", module->address) == NULL) {
            debug_log_fmt("[-] Failed to load %s\n", module->path);
        } else {
            debug_log_fmt("[+] Loaded \"%s\"...\n", module->path);
        }
    }
}
