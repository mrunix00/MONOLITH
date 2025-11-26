/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Page table entry flags.
 * Check Intel's x86 Developer Manual Volume 3A, Chapter 5.5.4.
 * https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
 */
typedef struct
{
    bool present : 1;
    bool read_write : 1;
    bool user : 1;
    bool page_write_through : 1;
    bool caching_disable : 1;
    bool accessed : 1;
    bool dirty : 1;
    bool huge_page : 1;
    bool global_page : 1;
    uint8_t _available : 3;
    uint64_t physical : 51;
    bool xd : 1;
} page_table_flags_t;

typedef union {
    page_table_flags_t flags;
    uint64_t raw;
} page_table_entry_t;

typedef struct
{
    page_table_entry_t entries[512];
} page_table_t;

#define PML_GET_INDEX(ADDR, LEVEL) \
    (((uint64_t) ADDR & ((uint64_t) 0x1ff << (12 + LEVEL * 9))) >> (12 + LEVEL * 9))

#define PML4_GET_INDEX(ADDR) PML_GET_INDEX(ADDR, 3)
#define PML3_GET_INDEX(ADDR) PML_GET_INDEX(ADDR, 2)
#define PML2_GET_INDEX(ADDR) PML_GET_INDEX(ADDR, 1)
#define PML1_GET_INDEX(ADDR) PML_GET_INDEX(ADDR, 0)
