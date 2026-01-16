/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/debug.h>
#include <kernel/fs/vfs.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/elf.h>
#include <kernel/tasking/loader.h>
#include <stdint.h>

task_t *load_elf(const char *path)
{
    file_t file = file_open(path);
    if (file.internal == NULL)
        return NULL;

    __asm__("cli");
    debug_log("[*] Loading ELF file...\n");
    elf64_header_t header;
    if (parse_elf_header(&file, &header) < 0) {
        debug_log("[-] ELF parsing error\n");
        __asm__("sti");
        return NULL;
    }
    debug_log("[*] Loaded ELF file\n");

    if (header.header.format != ELF_FORMAT_64BIT) {
        debug_log("[-] ELF is not 64-bit\n");
        __asm__("sti");
        return NULL;
    } else if (header.header.isa != ELF_ISA_X86_64) {
        debug_log_fmt("[-] ELF is not compatible with x86_64! header.isa = 0x%x\n", header.header.isa);
        __asm__("sti");
        return NULL;
    } else if (header.header.type != ELF_TYPE_EXEC) {
        debug_log("[-] ELF is not an executable\n");
        __asm__("sti");
        return NULL;
    }
    debug_log_fmt("[*] Found %d program headers\n", header.pht_entry_count);

    elf64_psh_t psh;
    task_t *task = task_create((void *) header.entry_offset, path, TASK_MODE_USER);

    for (int i = 0; i < header.pht_entry_count; i++) {
        if (file_seek(&file, header.pht_offset + i * header.pht_entry_size, SEEK_SET) < 0)
            return NULL;

        if (parse_elf_program_header(&file, &psh, header.pht_entry_size) < 0)
            return NULL;

        uintptr_t vaddr = psh.section_vaddr;
        uintptr_t vaddr_end = vaddr + psh.section_memory_size;
        uintptr_t vaddr_start = vaddr & ~(PAGE_SIZE - 1);
        uintptr_t vaddr_end_aligned = PAGE_UP(vaddr_end);
        size_t npages = (vaddr_end_aligned - vaddr_start) / PAGE_SIZE;

        void *phys_mem = pmm_alloc(npages);
        if (!phys_mem) {
            debug_log("[-] Failed to allocate physical memory for segment\n");
            __asm__("sti");
            return NULL;
        }

        uint64_t flags = PTFLAG_US | PTFLAG_P | PTFLAG_RW | PTFLAG_XD;
        task_map(task, vaddr_start, (uintptr_t) phys_mem, npages, flags, true);

        void *hddm_addr = vmm_get_hhdm_addr(phys_mem);
        memset(hddm_addr, 0, npages * PAGE_SIZE);

        size_t offset_in_page = vaddr - vaddr_start;
        if (file_seek(&file, psh.section_offset, SEEK_SET) < 0) {
            debug_log("[-] Failed to seek to segment offset\n");
            pmm_free(phys_mem, npages);
            __asm__("sti");
            return NULL;
        }

        if (file_read(&file, hddm_addr + offset_in_page, psh.section_file_size) < 0) {
            debug_log("[-] Failed to read segment data\n");
            pmm_free(phys_mem, npages);
            __asm__("sti");
            return NULL;
        }
    }

    void *stack = pmm_alloc(10);
    task_map(
        task, 0x00007fffe0000000ULL, (uintptr_t) stack, 10, PTFLAG_US | PTFLAG_RW | PTFLAG_P, true);

    /*
     * According to System V AMD64 ABI: RSP must be 16-byte aligned before CALL,
     * which means RSP % 16 == 8 at function entry (after return address is pushed).
     */
    uintptr_t stack_top = 0x00007fffe0000000ULL + 10 * PAGE_SIZE;
    task->state.rsp = stack_top - 8;

    __asm__("sti");
    return task;
}
