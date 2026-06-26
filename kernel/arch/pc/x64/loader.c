/* Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */
#include <kernel/arch/pc/interrupts.h>
#include <kernel/devices/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/elf.h>
#include <kernel/tasking/loader.h>
#include <shared/include/monolith/stdio.h>

static void _setup_user_stack_args(
    task_t *task,
    void *stack_phys,
    uintptr_t stack_base,
    uintptr_t stack_top,
    int argc,
    const char **argv)
{
    if (argc <= 0 || argv == NULL) {
        argc = 1;
        const char *default_argv[] = {""};
        argv = default_argv;
    }

    uint8_t *stack_hddm = (uint8_t *) vmm_get_hhdm_addr(stack_phys);

    size_t total_str_len = 0;
    for (int i = 0; i < argc; i++)
        total_str_len += strlen(argv[i]) + 1;
    total_str_len = (total_str_len + 7) & ~((size_t) 7);

    uintptr_t str_area_top = stack_top - total_str_len;

    /* Temporarily hold the user-virtual address of each argument string */
    uintptr_t *str_addrs = (uintptr_t *) kmalloc(sizeof(uintptr_t) * argc);
    if (!str_addrs)
        return;

    {
        uintptr_t p = str_area_top;
        for (int i = 0; i < argc; i++) {
            size_t len = strlen(argv[i]) + 1;
            str_addrs[i] = p;
            memcpy(stack_hddm + (p - stack_base), argv[i], len);
            p += len;
        }
    }

    size_t frame_size = 8 + 8 * argc + 16;
    uintptr_t sp = str_area_top - frame_size;
    if ((sp & 0xF) == 0)
        sp -= 8;
    uintptr_t wp = sp;

    /* argc */
    *(uint64_t *) (stack_hddm + (wp - stack_base)) = (uint64_t) argc;
    wp += 8;

    /* argv pointers */
    for (int i = 0; i < argc; i++) {
        *(uint64_t *) (stack_hddm + (wp - stack_base)) = str_addrs[i];
        wp += 8;
    }

    /* NULL terminator for argv */
    *(uint64_t *) (stack_hddm + (wp - stack_base)) = 0;
    wp += 8;

    /* NULL terminator for envp */
    *(uint64_t *) (stack_hddm + (wp - stack_base)) = 0;
    wp += 8;

    /* Set task state registers per System V AMD64 ABI */
    task->regs.rsp = sp;
    task->regs.rdi = argc;
    task->regs.rsi = sp + 8;
    task->regs.rdx = 0;

    kfree(str_addrs);
}

static task_t *_load_elf_impl(const char *path, task_t *parent, int argc, const char **argv)
{
    rsrc_t *file = rsmgr_open(path);
    if (file == NULL)
        return NULL;

    interrupts_disable();
    debug_log("Loading ELF file...\n");

    union {
        elf_header_t common;
        elf64_header_t elf64;
    } header;

    if (parse_elf_header(file, &header) < 0) {
        debug_log("ELF parsing error\n");
        interrupts_enable();
        return NULL;
    }
    debug_log("Loaded ELF file\n");

    if (header.common.format != ELF_FORMAT_64BIT) {
        debug_log("ELF is not 64-bit\n");
        interrupts_enable();
        return NULL;
    } else if (header.common.isa != ELF_ISA_X86_64) {
        debug_log_fmt(
            "ELF is not compatible with x86_64! header.isa = 0x%x\n", (uintptr_t) header.common.isa);
        interrupts_enable();
        return NULL;
    }
    uintptr_t entry_offset = header.elf64.entry_offset;
    uintptr_t pht_offset = header.elf64.pht_offset;
    uint16_t pht_entry_size = header.elf64.pht_entry_size;
    uint16_t pht_entry_count = header.elf64.pht_entry_count;

    if (header.common.type != ELF_TYPE_EXEC) {
        debug_log("ELF is not an executable\n");
        interrupts_enable();
        return NULL;
    }

    debug_log_fmt("Found %d program headers\n", pht_entry_count);

    task_t *task = task_create((void *) entry_offset, path, TASK_MODE_USER);

    for (int i = 0; i < pht_entry_count; i++) {
        elf64_psh_t psh;
        if (parse_elf_program_header(file, pht_offset + i * pht_entry_size, &psh, pht_entry_size)
            < 0) {
            interrupts_enable();
            return NULL;
        }

        if (psh.type != SECTION_TYPE_LOAD)
            continue;

        uintptr_t vaddr = psh.section_vaddr;
        uintptr_t vaddr_end = vaddr + psh.section_memory_size;
        uintptr_t vaddr_start = vaddr & ~(PAGE_SIZE - 1);
        uintptr_t vaddr_end_aligned = PAGE_UP(vaddr_end);
        size_t npages = (vaddr_end_aligned - vaddr_start) / PAGE_SIZE;

        void *phys_mem = pmm_alloc(npages);
        if (!phys_mem) {
            debug_log("Failed to allocate physical memory for segment\n");
            interrupts_enable();
            return NULL;
        }

        uint64_t flags = PTFLAG_US | PTFLAG_P;
        if (psh.flags & SECTION_FLAG_WRITE)
            flags |= PTFLAG_RW;
        if (!(psh.flags & SECTION_FLAG_EXEC))
            flags |= PTFLAG_XD;
        task_map(task, vaddr_start, (uintptr_t) phys_mem, npages, flags, true);

        void *hddm_addr = vmm_get_hhdm_addr(phys_mem);
        memset(hddm_addr, 0, npages * PAGE_SIZE);

        size_t offset_in_page = vaddr - vaddr_start;

        uint64_t offset = psh.section_offset;
        uint64_t bytes_read = 0;
        if (rsmgr_seek(file, &offset, (int64_t) psh.section_offset, SEEK_SET, &offset)
                != RSRC_STATUS_OK
            || rsmgr_read(file, &offset, hddm_addr + offset_in_page, psh.section_file_size, &bytes_read)
                   != RSRC_STATUS_OK
            || bytes_read < psh.section_file_size) {
            debug_log("Failed to read segment data\n");
            pmm_free(phys_mem, npages);
            interrupts_enable();
            return NULL;
        }
    }

    void *stack = pmm_alloc(10);
    uintptr_t stack_base = 0x00007fffe0000000ULL;
    uintptr_t stack_top = stack_base + 10 * PAGE_SIZE;
    task_map(task, stack_base, (uintptr_t) stack, 10, PTFLAG_US | PTFLAG_RW | PTFLAG_P, true);

    /* Set up argc/argv on the user stack per System V AMD64 ABI */
    _setup_user_stack_args(task, stack, stack_base, stack_top, argc, argv);

    task_set_parent(task, parent);

    interrupts_enable();
    return task;
}

task_t *load_exec(const char *path, task_t *parent, int argc, const char **argv)
{
    return _load_elf_impl(path, parent, argc, argv);
}
