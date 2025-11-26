/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Heap statistics and information.
 */
typedef struct
{
    size_t free_blocks;
    size_t used_blocks;
    size_t used_memory;
} heap_stats_t;

/*
 * Initialize the heap with the specified amount of pages.
 * Returns true if successful, false otherwise.
 */
bool heap_init(size_t pages);

/*
 * Allocates a block of memory of the specified size.
 * Returns a pointer to the allocated memory, or NULL if the allocation fails.
 */
void *kmalloc(size_t size);

/*
 * Reallocates a block of memory of the specified size.
 * Returns a pointer to the reallocated memory, or NULL if the reallocation fails.
 */
void *krealloc(void *pointer, size_t size);

/*
 * Frees a previously allocated memory block.
 * The pointer parameter must be a pointer previously returned by kmalloc.
 */
void kfree(void *pointer);

/*
 * Returns information about the heap.
 */
heap_stats_t heap_get_stats(void);
