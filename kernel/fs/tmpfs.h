/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/fs/vfs.h>

/*
 * Mount a new tmpfs drive.
 * Returns the drive pointer when successful, or NULL on error.
 */
vfs_drive_t *tmpfs_new_drive(const char *prefix);
