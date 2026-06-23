/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/mmap.h>
#include <kernel/devices/debug.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/mmap.h>

static uint8_t *_bitmap;
static uint8_t *_bitmap_end;
static void *_phys_memory_start;
static size_t _bitmap_size;
static size_t _bitmap_page_count;

extern char _data_end[];

static uintptr_t _align_up(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void _mark_pages_used(size_t start_page, size_t number_of_pages)
{
    for (size_t j = start_page; j < start_page + number_of_pages; j++) {
        size_t byte_idx = j / 8;
        size_t bit_idx = j % 8;
        _bitmap[byte_idx] |= (1 << bit_idx);
    }
}

static void *_allocate_pages(size_t start_page, size_t number_of_pages)
{
    void *base_addr = _phys_memory_start + start_page * PAGE_SIZE;
    _mark_pages_used(start_page, number_of_pages);
    return (void *) base_addr;
}

static inline void *_process_byte(
    size_t byte, size_t *free_pages, size_t *start_page, size_t number_of_pages)
{
    size_t page_index = byte * 8;

    /* Skip scanning fully free bytes */
    if (_bitmap[byte] == 0x00) {
        if (*free_pages == 0) {
            *start_page = page_index;
        }
        *free_pages += 8;
        if (*free_pages >= number_of_pages) {
            return _allocate_pages(*start_page, number_of_pages);
        } else {
            return NULL;
        }
    }

    for (size_t bit = 0; bit < 8 && page_index + bit < _bitmap_page_count; bit++) {
        size_t i = page_index + bit;
        uint8_t bit_mask = 1 << bit;

        if (!(_bitmap[byte] & bit_mask)) {
            if (*free_pages == 0) {
                *start_page = i;
            }
            (*free_pages)++;

            if (*free_pages == number_of_pages) {
                return _allocate_pages(*start_page, number_of_pages);
            }
        } else {
            *free_pages = 0;
        }
    }

    return NULL;
}

static const char *_get_mmap_type(uint32_t t)
{
    switch (t) {
    case MMAP_USABLE:
        return "Usable";
    case MMAP_RESERVED:
        return "Reserved";
    case MMAP_ACPI_RECLAIMABLE:
        return "ACPI Reclaimable";
    case MMAP_ACPI_NVS:
        return "ACPI NVS";
    case MMAP_BAD_MEMORY:
        return "Bad Memory";
    case MMAP_BOOTLOADER_RECLAIMABLE:
        return "Bootloader Reclaimable";
    case MMAP_EXECUTABLE_AND_MODULES:
        return "Kernel and Modules";
    case MMAP_FRAMEBUFFER:
        return "Framebuffer";
    default:
        return "Unknown";
    }
}

void pmm_init(void)
{
    debug_log("Initializing PMM\n");

    debug_log_fmt("Number of memory map entries: %d\n", mmap_entry_count);

    /* Calculate reserved boot/kernel end and usable physical memory size. */
    uintptr_t kernel_end_addr = _align_up((uintptr_t) _data_end, PAGE_SIZE);
    const mmap_entry_t *biggest = NULL;
    size_t physical_memory_size = 0;
    for (size_t i = 0; i < mmap_entry_count; i++) {
        const mmap_entry_t *entry = &mmap_entries[i];
        debug_log_fmt(
            "\tMmap Entry %d (type: %s, base: 0x%x, length: %d bytes)\n",
            i,
            _get_mmap_type(entry->type),
            (uint32_t) entry->base,
            (uint32_t) entry->length);
        if (entry->type == MMAP_EXECUTABLE_AND_MODULES) {
            uintptr_t entry_end = (uintptr_t) (entry->base + entry->length);
            if (entry_end > kernel_end_addr)
                kernel_end_addr = _align_up(entry_end, PAGE_SIZE);
        }
        if (entry->type == MMAP_USABLE) {
            if (biggest == NULL || entry->length > biggest->length)
                biggest = entry;
            physical_memory_size += entry->length;
        }
    }
    debug_log_fmt("Found %d MB of physical memory\n", physical_memory_size / 1048576);

    uintptr_t bitmap_phys = (uintptr_t) biggest->base;
    uintptr_t biggest_end = (uintptr_t) (biggest->base + biggest->length);
    if (bitmap_phys < kernel_end_addr && kernel_end_addr < biggest_end)
        bitmap_phys = kernel_end_addr;

    _bitmap = vmm_get_hhdm_addr((void *) bitmap_phys);
    _bitmap_page_count = physical_memory_size / PAGE_SIZE;
    _bitmap_size = (_bitmap_page_count + 7) / 8; /* Round up division */
    _bitmap_end = _bitmap + _bitmap_size;
    _phys_memory_start = (void *) _align_up((uintptr_t) vmm_get_lhdm_addr(_bitmap_end), PAGE_SIZE);

    debug_log_fmt("Physical memory start: 0x%x\n", (uintptr_t) _phys_memory_start);
    debug_log_fmt("End of kernel address: 0x%x\n", kernel_end_addr);
    debug_log_fmt("Bitmap address range: 0x%x-0x%x\n", (uintptr_t) _bitmap, (uintptr_t) _bitmap_end);
    debug_log_fmt("Bitmap page count: %d pages\n", _bitmap_page_count);
    debug_log_fmt("Bitmap size: %d bytes\n", _bitmap_size);

    debug_log("Initializing the bitmap...\n");
    for (size_t i = 0; i < _bitmap_size; i++)
        _bitmap[i] = 0;

    debug_log("PMM initialized\n");
}

void *pmm_alloc(size_t pages)
{
    size_t current_free_pages = 0;
    size_t start_page = 0;
    void *result = NULL;

    for (size_t byte = 0; byte < _bitmap_size; byte++) {
        /* Skip fully used bytes */
        if (_bitmap[byte] == 0xFF) {
            current_free_pages = 0;
            continue;
        }

        result = _process_byte(byte, &current_free_pages, &start_page, pages);
        if (result) {
            return result;
        }
    }

    debug_log_fmt("pmm_alloc failed: Could not find %d contiguous pages\n", pages);
    return NULL;
}

void pmm_free(void *ptr, size_t pages)
{
    if (ptr == NULL)
        return;

    uintptr_t base_addr = (uint64_t) ptr;
    if (base_addr % PAGE_SIZE != 0) {
        debug_log_fmt("[!] pmm_free: Pointer 0x%x is not page-aligned\n", (uintptr_t) ptr);
        return;
    }
    size_t start_page = (base_addr - (uintptr_t) _phys_memory_start) / PAGE_SIZE;
    if (start_page + pages > _bitmap_page_count) {
        debug_log_fmt(
            "[!] pmm_free: Freeing %d pages at 0x%x exceeds bitmap\n", pages, (uintptr_t) ptr);
        return;
    }

    /* Mark the pages as free */
    for (size_t i = start_page; i < start_page + pages; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        _bitmap[byte_idx] &= ~(1 << bit_idx);
    }
}
