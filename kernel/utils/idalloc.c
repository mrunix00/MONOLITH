/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/utils/idalloc.h>
#include <stdint.h>

bool idalloc_init(idalloc_t *alloc, size_t capacity)
{
    alloc->bitmap = kmalloc(capacity / 8);
    if (alloc->bitmap == NULL)
        return false;
    alloc->size = capacity;
    alloc->used_words = 0;
    memset(alloc->bitmap, 0, capacity);
    return true;
}

uint64_t idalloc_next(idalloc_t *alloc)
{
    if (alloc->used_words / 8 == alloc->size) {
        uint8_t *new_bitmap = krealloc(alloc->bitmap, alloc->size * 2);
        if (new_bitmap == NULL)
            return UINT64_MAX;
        memset(new_bitmap + alloc->size, 0, alloc->size);
        alloc->bitmap = new_bitmap;
        alloc->size *= 2;
    }
    for (uint64_t i = 0; i < alloc->size; i++) {
        if (alloc->bitmap[i] == 0xFF)
            continue;
        for (uint64_t j = 0; j < 8; j++) {
            if ((alloc->bitmap[i] >> j) & 1)
                continue;
            alloc->bitmap[i] |= (1 << j);
            alloc->used_words++;
            return i * 8 + j;
        }
    }
    return UINT64_MAX;
}

void idalloc_free(idalloc_t *alloc, uint64_t id)
{
    if (id >= alloc->size * 8)
        return;
    uint64_t word = id / 8;
    uint64_t bit = id % 8;
    alloc->bitmap[word] &= ~(1 << bit);
    alloc->used_words--;
}

void idalloc_deinit(idalloc_t *alloc)
{
    kfree(alloc->bitmap);
}
