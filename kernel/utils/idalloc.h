/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t *bitmap;
    size_t size;
    size_t used_words;
} idalloc_t;

/*
 * Initializes the id allocator with the given capacity.
 * Returns true on success, false on failure.
 */
bool idalloc_init(idalloc_t *alloc, size_t capacity);

/*
 * Allocates a new id and returns it.
 * Returns UINT64_MAX on failure.
 */
uint64_t idalloc_next(idalloc_t *alloc);

/*
 * Frees the given id.
 */
void idalloc_free(idalloc_t *alloc, uint64_t id);

/*
 * Deinitializes the id allocator.
 */
void idalloc_deinit(idalloc_t *alloc);
