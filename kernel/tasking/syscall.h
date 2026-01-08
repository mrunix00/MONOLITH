/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Flags for sys_alloc_pages.
 */
#define ALLOC_PAGES_FLAG_RW (1 << 0)   /* Read/Write access (default is read-only) */
#define ALLOC_PAGES_FLAG_EXEC (1 << 1) /* Executable (default is non-executable) */

void syscalls_init();
