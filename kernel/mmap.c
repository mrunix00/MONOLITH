/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/mmap.h>

mmap_entry_t mmap_entries[MAX_MMAP_ENTRIES];
uint32_t mmap_entry_count = 0;

bool mmap_add_entry(uint64_t base, uint64_t length, mmap_type_t type)
{
    if (mmap_entry_count >= MAX_MMAP_ENTRIES)
        return false;
    mmap_entries[mmap_entry_count++] = (mmap_entry_t){base, length, type};
    return true;
}
