/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/fs/vfs.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Load and execute an executable file into memory.
 * Returns 0 on success, -1 on failure.
 */
int load_elf(file_t *file);
