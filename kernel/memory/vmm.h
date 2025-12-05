/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libs/limine-protocol/include/limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Page table flags for x86_64.
 * Check Intel's x86 Developer Manual Volume 3A, Section 5.5.4, Table 5-20.
 * https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
 */
typedef enum : uint64_t {
    PTFLAG_XD = 1 << 63, /* Execute Disable */
    PTFLAG_R = 1 << 11,  /* Restart HLAT Paging (See Intel's x86 Manual Volume 3A, Section 5.5.5) */
    PTFLAG_G = 1 << 8,   /* Global (See Intel's x86 Manual Volume 3A, Section 5.10) */
    PTFLAG_PAT = 1 << 7, /* Page Attribute Table (See Intel's x86 Manual Volume 3A, Section 5.9.2) */
    PTFLAG_D = 1 << 6,   /* Dirty (See Intel's x86 Manual Volume 3A, Section 5.8) */
    PTFLAG_A = 1 << 5,   /* Accessed (See Intel's x86 Manual Volume 3A, Section 5.8) */
    PTFLAG_PCD = 1 << 4, /* Page Cache Disable (See Intel's x86 Manual Volume 3A, Section 5.9.2) */
    PTFLAG_PWT = 1 << 3, /* Page Write Through (See Intel's x86 Manual Volume 3A, Section 5.9.2) */
    PTFLAG_US = 1 << 2,  /* User/Supervisor (See Intel's x86 Manual Volume 3A, Section 5.6) */
    PTFLAG_RW = 1 << 1,  /* Read/Write (See Intel's x86 Manual Volume 3A, Section 5.6) */
    PTFLAG_P = 1 << 0,   /* Present */
} pt_flags_t;

/*
 * Initialize the virtual memory manager.
 */
void vmm_init(struct limine_memmap_response *);

/*
 * Map a single page from a physical to a virtual address.
 * cr3 is the physical address of the target PML4.
 */
void vmm_map(uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t flags, bool flush);

/*
 * Map a specified number of pages from a physical to a virtual address.
 * cr3 is the physical address of the target PML4.
 */
void vmm_map_range(uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t size, size_t flags, bool flush);

/*
 * Unmap a single page from a virtual address.
 * cr3 is the physical address of the target PML4.
 */
void vmm_unmap(uintptr_t cr3, uintptr_t virt_addr, bool flush);

/*
 * Unmap a specified number of pages from a virtual address.
 * cr3 is the physical address of the target PML4.
 */
void vmm_unmap_range(uintptr_t cr3, uintptr_t virt_addr, size_t size, bool flush);

/*
 * Convert a physical address to a high-half direct mapped address.
 */
void *vmm_get_hhdm_addr(void *phys_addr);

/*
 * Convert a higher-half direct mapped virtual address to the physical address.
 */
void *vmm_get_lhdm_addr(void *virt_addr);

/*
 * Create a new address space for a task.
 * Returns the physical address of the new PML4 (suitable for loading into CR3).
 * The higher half (kernel space) is shared with the kernel's address space.
 * Returns 0 on failure.
 */
uintptr_t vmm_create_address_space(void);

/*
 * Destroy an address space, freeing all userspace page tables.
 * Does not free the actual physical pages that were mapped - only the page table structures.
 * The cr3 parameter is the physical address of the PML4 to destroy.
 * Does nothing if cr3 is the kernel's address space.
 */
void vmm_destroy_address_space(uintptr_t cr3);

/*
 * Get the kernel's CR3 value (physical address of the kernel's PML4).
 */
uintptr_t vmm_get_kernel_cr3(void);