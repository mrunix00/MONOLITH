/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_MMAP_ENTRIES 256

typedef enum : uint32_t {
    MMAP_USABLE = 0,
    MMAP_RESERVED = 1,
    MMAP_ACPI_RECLAIMABLE = 2,
    MMAP_ACPI_NVS = 3,
    MMAP_BAD_MEMORY = 4,
    MMAP_BOOTLOADER_RECLAIMABLE = 5,
    MMAP_EXECUTABLE_AND_MODULES = 6,
    MMAP_FRAMEBUFFER = 7,
} mmap_type_t;

typedef struct
{
    uint64_t base;
    uint64_t length;
    mmap_type_t type;
} mmap_entry_t;

bool mmap_add_entry(uint64_t base, uint64_t length, mmap_type_t type);

extern mmap_entry_t mmap_entries[];
extern uint32_t mmap_entry_count;
