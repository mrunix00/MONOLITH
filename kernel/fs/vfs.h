/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PATH_MAX 4096

typedef enum {
    FILE,
    DIRECTORY,
} file_type_t;

typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} seek_mode_t;

/*
 * Directory entry structure.
 */
typedef struct
{
    size_t length;    /* Length of the current directory entry */
    file_type_t type; /* File type */
    char name[];      /* File name */
} dir_entry_t;

/*
 * File info structure.
 */
typedef struct
{
    uint64_t size;
    file_type_t type;
} file_stats_t;

/*
 * Virtual file system node structure.
 */
typedef struct vfs_node
{
    char *name;               /* File name */
    file_type_t type;         /* File type (File/Directory) */
    struct vfs_node *parent;  /* Node parent */
    struct vfs_node *child;   /* Node's first child */
    struct vfs_node *sibling; /* Node's next sibling */
    void *internal;           /* Internal inode data */
} vfs_node_t;

/*
 * File object structure.
 */
typedef struct
{
    struct vfs_drive *drive; /* Drive where the file is located */
    void *internal;          /* Internal file data */
    uint32_t offset;         /* Current read/write offset */
} file_t;

/*
 * File system drive structure.
 */
typedef struct vfs_drive
{
    uint8_t id;     /* Drive index (0-255) */
    char name[40];  /* Drive name */
    void *internal; /* Internal drive data */
    file_t (*open)(struct vfs_drive *drive, const char *path);
    int (*create)(struct vfs_drive *drive, const char *name, file_type_t type);
    int (*remove)(struct vfs_drive *drive, const char *name);
    int (*read)(file_t *file, void *buffer, uint32_t size);
    int (*write)(file_t *file, const void *buffer, uint32_t size);
    int (*seek)(file_t *file, size_t offset, seek_mode_t mode);
    int (*getdents)(file_t *file, void *buffer, uint32_t size);
    int (*getstats)(file_t *file, file_stats_t *stats);
    size_t (*tell)(file_t *file);
} vfs_drive_t;

/*
 * Create a new drive.
 * Returns a pointer to the new drive, or NULL on failure.
 */
vfs_drive_t *vfs_new_drive(const char *prefix);

/*
 * Remove a drive by the drive pointer.
 */
void vfs_remove_drive(vfs_drive_t *drive);

/*
 * Remove a drive by the drive name.
 */
void vfs_remove_drive_by_name(const char *name);

/*
 * Add a child node to a parent node.
 */
void vfs_add_child(vfs_node_t *parent, vfs_node_t *child);
/*
 * Remove a child node from a parent node.
 */
void vfs_remove_child(vfs_node_t *parent, vfs_node_t *child);

/*
 * Get the relative path of a node from a parent node, returns NULL on failure.
 */
vfs_node_t *vfs_get_relative_path(vfs_node_t *parent, const char *path);

/*
 * Opens a file.
 * Returns a file handle with proper data on success, or a file handle with NULL data on failure.
 */
file_t file_open(const char *path);

/*
 * Creates a new file.
 * Returns 0 on success, or -1 on failure.
 */
int file_create(const char *path, file_type_t type);

/*
 * Removes a file.
 * Returns 0 on success, or -1 on failure.
 */
int file_remove(const char *path);

/*
 * Reads data from a file.
 * Returns the number of bytes read, or -1 on failure.
 */
int file_read(file_t *file, void *buffer, uint32_t size);

/*
 * Writes data to a file.
 * Returns the number of bytes written, or -1 on failure.
 */
int file_write(file_t *file, const void *buffer, uint32_t size);

/*
 * Seeks to a position in a file.
 * Returns 0 on success, or -1 on failure.
 */
int file_seek(file_t *file, size_t offset, seek_mode_t mode);

/*
 * Reads directory entries from a file.
 * Returns the number of bytes read, or -1 on failure.
 */
int file_getdents(file_t *file, void *buffer, uint32_t size);

/*
 * Gets file statistics.
 * Returns 0 on success, or -1 on failure.
 */
int file_getstats(file_t *file, file_stats_t *stats);

/*
 * Gets the current position in a file.
 * Returns the current position, or -1 on failure.
 */
size_t file_tell(file_t *file);

/*
 * Lists available drives.
 * Returns the number of bytes written to buffer, or -1 on failure.
 */
int vfs_getdrives(void *buffer, uint32_t size);
