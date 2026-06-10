/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/mmap.h>
#include <kernel/arch/pc/x64/paging.h>
#include <kernel/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <libs/limine-protocol/include/limine.h>

__attribute__((used, section(".limine_requests"))) volatile struct limine_hhdm_request
    limine_hhdm_request = {.id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_executable_address_request
    limine_kernel_address_request = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_paging_mode_request
    limine_paging_request
    = {.id = LIMINE_PAGING_MODE_REQUEST_ID, .mode = LIMINE_PAGING_MODE_X86_64_4LVL, .revision = 0};

static page_table_t *_pt_top_level;

extern void *_limine_requests_start;
extern void *_limine_requests_end;
extern void *_text_start;
extern void *_text_end;
extern void *_data_start;
extern void *_data_end;
extern void *_rodata_start;
extern void *_rodata_end;

void *vmm_get_hhdm_addr(void *phys_addr)
{
    return limine_hhdm_request.response->offset + phys_addr;
}

void *vmm_get_lhdm_addr(void *virt_addr)
{
    return virt_addr - limine_hhdm_request.response->offset;
}

uintptr_t vmm_get_kernel_cr3(void)
{
    return (uintptr_t) vmm_get_lhdm_addr(_pt_top_level);
}

static inline void _asm_write_msr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t) value;
    uint32_t high = (uint32_t) (value >> 32);
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline void _asm_cpuid(uint32_t eax, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(eax));
}

static void _set_pat(void)
{
    uint32_t eax, ebx, ecx, edx;
    _asm_cpuid(1, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1 << 16))) {
        debug_log("PAT not supported. WC mappings may not work.\n");
        return;
    }

    /*
     * Configure PAT entries:
     * PAT0: WB (0x06), PAT1: WT (0x04), PAT2: UC- (0x07), PAT3: UC (0x00)
     * PAT4: WC (0x01), PAT5: WP (0x05), PAT6: UC- (0x07), PAT7: UC (0x00)
     */
    uint64_t pat_value = 0x0007050100070406;
    _asm_write_msr(0x277, pat_value);
}

static inline void *_get_next_level(page_table_t *pt, uint64_t index)
{
    page_table_entry_t *entry = &pt->entries[index];
    void *next_pt;
    if (!entry->flags.present) {
        next_pt = pmm_alloc(1);
        if (next_pt == NULL)
            return NULL;
        entry->raw = ((uint64_t) next_pt) | 0b111;
        page_table_t *next_pt_hhdm = vmm_get_hhdm_addr(next_pt);
        memset(next_pt_hhdm, 0, PAGE_SIZE);
        return next_pt_hhdm;
    }
    return vmm_get_hhdm_addr((void *) (entry->raw & ~0xFFF));
}

void vmm_init(void)
{
    debug_log("Initializing VMM...\n");
    debug_log("Using Level-4 paging\n");

    _pt_top_level = pmm_alloc(1);
    if (_pt_top_level == NULL) {
        debug_log_fmt("Failed to initialize the VMM");
        while (1)
            __asm__("hlt");
    }
    _pt_top_level = vmm_get_hhdm_addr(_pt_top_level);
    memset(_pt_top_level, 0, PAGE_SIZE);

    uintptr_t kernel_cr3 = vmm_get_kernel_cr3();

    for (size_t i = 0; i < mmap_entry_count; i++) {
        const mmap_entry_t entry = mmap_entries[i];
        if (entry.type != MMAP_BAD_MEMORY) {
            vmm_map_range(
                kernel_cr3,
                (uintptr_t) vmm_get_hhdm_addr((void *) entry.base),
                entry.base,
                entry.length,
                0x0000000000000003,
                false);
        }
    }

    uintptr_t kernel_paddr = limine_kernel_address_request.response->physical_base;
    uintptr_t kernel_vaddr = limine_kernel_address_request.response->virtual_base;

    uintptr_t limine_requests_start_vaddr = (uintptr_t) &_limine_requests_start;
    uintptr_t limine_requests_start_paddr = limine_requests_start_vaddr - kernel_vaddr
                                            + kernel_paddr;
    uintptr_t limine_requests_size = (uintptr_t) &_limine_requests_end
                                     - limine_requests_start_vaddr;
    uintptr_t text_start_vaddr = (uintptr_t) &_text_start;
    uintptr_t text_start_paddr = text_start_vaddr - kernel_vaddr + kernel_paddr;
    uintptr_t text_size = (uintptr_t) &_text_end - text_start_vaddr;
    uintptr_t rodata_start_vaddr = (uintptr_t) &_rodata_start;
    uintptr_t rodata_start_paddr = rodata_start_vaddr - kernel_vaddr + kernel_paddr;
    uintptr_t rodata_size = (uintptr_t) &_rodata_end - rodata_start_vaddr;
    uintptr_t data_start_vaddr = (uintptr_t) &_data_start;
    uintptr_t data_start_paddr = data_start_vaddr - kernel_vaddr + kernel_paddr;
    uintptr_t data_size = (uintptr_t) &_data_end - data_start_vaddr;

    vmm_map_range(
        kernel_cr3,
        limine_requests_start_vaddr,
        limine_requests_start_paddr,
        limine_requests_size,
        0x03,
        false);
    vmm_map_range(kernel_cr3, text_start_vaddr, text_start_paddr, text_size, 0x03, false);
    vmm_map_range(kernel_cr3, rodata_start_vaddr, rodata_start_paddr, rodata_size, 0x03, false);
    vmm_map_range(kernel_cr3, data_start_vaddr, data_start_paddr, data_size, 0x03, false);

    _set_pat();
    asm_write_cr3(kernel_cr3);

    debug_log_fmt("The page table is located at 0x%x\n", (uintptr_t) _pt_top_level);
    debug_log("Initialized VMM\n");
}

void vmm_map(uintptr_t cr3, uintptr_t virt, uintptr_t phys, size_t flags, bool flush)
{
    page_table_t *pml4 = vmm_get_hhdm_addr((void *) cr3);

    uint64_t pml4_index = PML4_GET_INDEX(virt);
    uint64_t pdpt_index = PML3_GET_INDEX(virt);
    uint64_t pdt_index = PML2_GET_INDEX(virt);
    uint64_t pt_index = PML1_GET_INDEX(virt);

    page_table_t *pdpt, *pdt, *pt;

    pdpt = _get_next_level(pml4, pml4_index);
    if (pdpt == NULL)
        goto failure;

    pdt = _get_next_level(pdpt, pdpt_index);
    if (pdt == NULL)
        goto failure;

    pt = _get_next_level(pdt, pdt_index);
    if (pt == NULL)
        goto failure;

    pt->entries[pt_index].raw = (uintptr_t) phys | flags;
    if (flush) {
        debug_log_fmt("Mapped phys 0x%x to virt 0x%x\n", phys, virt);
        if (asm_read_cr3() == cr3) {
            asm_invlpg((void *) virt);
        }
    }
    return;

failure:
    debug_log_fmt(
        "Failed to map virtual address 0x%x to physical address 0x%x in address space 0x%x\n",
        virt,
        phys,
        cr3);
}

void vmm_map_range(
    uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t size, size_t flags, bool flush)
{
    debug_log_fmt(
        "Mapping 0x%x - 0x%x to 0x%x - 0x%x in address space 0x%x\n",
        phys_addr,
        phys_addr + size,
        virt_addr,
        virt_addr + size,
        cr3);
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE; /* Round up */
    for (size_t i = 0; i < num_pages; i++)
        vmm_map(cr3, virt_addr + i * PAGE_SIZE, phys_addr + i * PAGE_SIZE, flags, false);
    if (flush && asm_read_cr3() == cr3)
        asm_write_cr3(cr3);
}

void vmm_unmap(uintptr_t cr3, uintptr_t virt, bool flush)
{
    page_table_t *pml4 = vmm_get_hhdm_addr((void *) cr3);

    uint64_t pml4_index = PML4_GET_INDEX(virt);
    uint64_t pdpt_index = PML3_GET_INDEX(virt);
    uint64_t pdt_index = PML2_GET_INDEX(virt);
    uint64_t pt_index = PML1_GET_INDEX(virt);

    page_table_t *pdpt, *pdt, *pt;

    if (!pml4->entries[pml4_index].flags.present)
        return;
    pdpt = vmm_get_hhdm_addr((void *) (pml4->entries[pml4_index].raw & ~0xFFF));

    if (!pdpt->entries[pdpt_index].flags.present)
        return;
    pdt = vmm_get_hhdm_addr((void *) (pdpt->entries[pdpt_index].raw & ~0xFFF));

    if (!pdt->entries[pdt_index].flags.present)
        return;
    pt = vmm_get_hhdm_addr((void *) (pdt->entries[pdt_index].raw & ~0xFFF));

    pt->entries[pt_index].raw = 0;
    if (flush && asm_read_cr3() == cr3)
        asm_invlpg((void *) virt);
}

void vmm_unmap_range(uintptr_t cr3, uintptr_t virt_addr, size_t size, bool flush)
{
    debug_log_fmt("Unmapping 0x%x - 0x%x from address space 0x%x\n", virt_addr, virt_addr + size, cr3);
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE; /* Round up */
    for (size_t i = 0; i < num_pages; i++) {
        vmm_unmap(cr3, virt_addr + i * PAGE_SIZE, false);
    }
    if (flush && asm_read_cr3() == cr3)
        asm_write_cr3(cr3);
}

uintptr_t vmm_create_address_space(void)
{
    /* Allocate a new PML4 */
    void *new_pml4_phys = pmm_alloc(1);
    if (new_pml4_phys == NULL) {
        debug_log("Failed to allocate PML4 for new address space\n");
        return 0;
    }

    page_table_t *new_pml4 = vmm_get_hhdm_addr(new_pml4_phys);
    memset(new_pml4, 0, PAGE_SIZE);

    /* Copy the higher half (kernel space) entries from the kernel's PML4.*/
    for (int i = 256; i < 512; i++)
        new_pml4->entries[i] = _pt_top_level->entries[i];

    debug_log_fmt("Created new address space at 0x%x\n", (uintptr_t) new_pml4_phys);
    return (uintptr_t) new_pml4_phys;
}

static void _free_page_table_level(page_table_t *pt, int level)
{
    if (pt == NULL || level < 1)
        return;

    for (int i = 0; i < 512; i++) {
        if (pt->entries[i].flags.present && level > 1) {
            /* Recurse into lower levels */
            page_table_t *next = vmm_get_hhdm_addr((void *) (pt->entries[i].raw & ~0xFFF));
            _free_page_table_level(next, level - 1);

            /* Free this page table page (but not the actual mapped memory at level 1) */
            void *phys = (void *) (pt->entries[i].raw & ~0xFFF);
            pmm_free(phys, 1);
        }
    }
}

void vmm_destroy_address_space(uintptr_t cr3)
{
    /* Don't destroy the kernel's address space */
    if (cr3 == vmm_get_kernel_cr3() || cr3 == 0)
        return;

    page_table_t *pml4 = vmm_get_hhdm_addr((void *) cr3);

    /* Only free the lower half (userspace) page tables. */
    for (int i = 0; i < 256; i++) {
        if (pml4->entries[i].flags.present) {
            page_table_t *pdpt = vmm_get_hhdm_addr((void *) (pml4->entries[i].raw & ~0xFFF));
            _free_page_table_level(pdpt, 3); /* Level 3 = PDPT */
            void *phys = (void *) (pml4->entries[i].raw & ~0xFFF);
            pmm_free(phys, 1);
        }
    }

    /* Free the PML4 itself */
    pmm_free((void *) cr3, 1);
    debug_log_fmt("Destroyed address space at 0x%x\n", cr3);
}
