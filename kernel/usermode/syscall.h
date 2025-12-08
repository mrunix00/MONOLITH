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

/*
 * Allocate and map a specified number of pages into the current task's address space.
 * Returns the virtual address of the allocated pages, or NULL on failure.
 *
 * Parameters:
 *   num_pages - Number of pages to allocate
 *   flags     - Allocation flags (ALLOC_PAGES_FLAG_*)
 */
void *sys_alloc_pages(size_t num_pages, uint64_t flags);
