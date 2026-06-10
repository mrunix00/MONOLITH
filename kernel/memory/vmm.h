/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PTFLAG_XD  ((uintptr_t) 1ULL << ((sizeof(uintptr_t) * 8) - 1))
#define PTFLAG_R   ((uintptr_t) 1 << 11)
#define PTFLAG_G   ((uintptr_t) 1 << 8)
#define PTFLAG_PAT ((uintptr_t) 1 << 7)
#define PTFLAG_D   ((uintptr_t) 1 << 6)
#define PTFLAG_A   ((uintptr_t) 1 << 5)
#define PTFLAG_PCD ((uintptr_t) 1 << 4)
#define PTFLAG_PWT ((uintptr_t) 1 << 3)
#define PTFLAG_US  ((uintptr_t) 1 << 2)
#define PTFLAG_RW  ((uintptr_t) 1 << 1)
#define PTFLAG_P   ((uintptr_t) 1 << 0)

void vmm_init(void);
void vmm_map(uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t flags, bool flush);
void vmm_map_range(uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t size, size_t flags, bool flush);
void vmm_unmap(uintptr_t cr3, uintptr_t virt_addr, bool flush);
void vmm_unmap_range(uintptr_t cr3, uintptr_t virt_addr, size_t size, bool flush);
void *vmm_get_hhdm_addr(void *phys_addr);
void *vmm_get_lhdm_addr(void *virt_addr);
uintptr_t vmm_create_address_space(void);
void vmm_destroy_address_space(uintptr_t cr3);
uintptr_t vmm_get_kernel_cr3(void);
