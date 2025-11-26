/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/fs/vfs.h>
#include <libs/limine-protocol/include/limine.h>

/*
 * Mount a new initrd drive.
 * Returns the drive ID when successful, or -1 on error.
 */
vfs_drive_t *initrd_new_drive(const char *prefix, void *data);

/*
 * Mount all available initrd drives from limine modules.
 */
void initrd_load_modules(struct limine_module_response *response);
