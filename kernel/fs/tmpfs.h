/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/rsmgr/rsmgr.h>

/*
 * Mount a new tmpfs collection under the file namespace.
 * Returns the root node when successful, or NULL on error.
 */
rsrc_node_t *tmpfs_mount(const char *name);

/*
 * Populate a tmpfs mount with contents from a USTAR initrd archive.
 * Returns 0 on success, or -1 on error.
 */
int tmpfs_populate_from_initrd(rsrc_node_t *root, void *ustar_data);

/*
 * Create a new resource in tmpfs.
 * Returns the new resource on success, or NULL on error.
 */
rsrc_t *tmpfs_new_resource(const char *name, rsrc_type_t type);

/*
 * Create a file or directory in tmpfs.
 * Returns 0 on success, or -1 on error.
 */
int tmpfs_create(rsrc_node_t *root, const char *path, rsrc_type_t type);

/*
 * Read data from a tmpfs resource.
 * Returns the number of bytes read, or -1 on error.
 */
int tmpfs_read(rsrc_t *resource, uint64_t offset, void *buffer, uint64_t size);

/*
 * Write data to a tmpfs resource.
 * Returns the number of bytes written, or -1 on error.
 */
int tmpfs_write(rsrc_t *resource, uint64_t offset, const void *buffer, uint64_t size);

/*
 * List directory entries from a tmpfs collection.
 * Returns the number of bytes written to buffer, or -1 on error.
 */
int tmpfs_list(rsrc_t *resource, uint64_t cursor, void *buffer, uint64_t buffer_len);
