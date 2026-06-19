/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

#define PAGE_SIZE 4096

/*
 * Aligns a given address to the nearest higher page boundary.
 */
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/*
 * Aligns a given address to the nearest lower page boundary.
 */
#define PAGE_DOWN(x) ((x) & ~(PAGE_SIZE - 1))

/*
 * Initialize the Physical Memory Manager.
 */
void pmm_init();

/*
 * Allocate free pages from physical memory.
 * Returns a pointer to the allocated page if successful, otherwise NULL.
 */
void *pmm_alloc(size_t pages);

/*
 * Free a page of physical memory.
 * Does nothing if the parameter is NULL.
 */
void pmm_free(void *, size_t);
