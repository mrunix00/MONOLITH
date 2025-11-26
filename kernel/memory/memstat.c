/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/memstat.h>
#include <kernel/memory/pmm.h>
#include <kernel/terminal/kshell.h>
#include <kernel/terminal/terminal.h>

static void _free(int argc, char *argv[])
{
    if (argc != 2) {
        kprintf("\n[*] Usage: free <address>");
        return;
    }

    /* TODO: add error handling */
    void *ptr = (void *) atox(argv[1]);
    kfree(ptr);

    kprintf("\n[+] Freed memory at 0x%x", ptr);
}

static void _alloc(int argc, char *argv[])
{
    if (argc != 2) {
        kprintf("\n[*] Usage: alloc <size in bytes>");
        return;
    }

    /* TODO: add error handling */
    void *ptr = kmalloc(atoul(argv[1]));
    if (!ptr) {
        kprintf("[-] Failed to allocate memory");
        return;
    }

    kprintf("\n[+] Allocated %s bytes at 0x%x", argv[1], ptr);
}

static void _memstat_print_stats(int, char **)
{
    pmm_stats_t pmm_info = pmm_get_stats();
    heap_stats_t heap_stats = heap_get_stats();

    size_t total_memory_mb = pmm_info.total_memory / 1048576;
    kprintf("\n[*] Total physical memory: %d MB", total_memory_mb);
    kprintf("\n[*] Total physical memory pages: %d pages", pmm_info.total_pages);
    kprintf("\n[*] Free physical memory pages: %d pages", pmm_info.free_pages);
    kprintf(
        "\n[*] Used physical memory pages: %d pages\n", pmm_info.total_pages - pmm_info.free_pages);

    kprintf("\n[*] Total heap blocks: %d blocks", heap_stats.free_blocks + heap_stats.used_blocks);
    kprintf("\n[*] Free heap blocks: %d blocks", heap_stats.free_blocks);
    kprintf("\n[*] Used heap blocks: %d blocks", heap_stats.used_blocks);
    kprintf("\n[*] Allocated heap memory: %d bytes", heap_stats.used_memory);
}

void memstat_init_cmds()
{
    kshell_register_command("memstat", "Show memory statistics", _memstat_print_stats);
    kshell_register_command("alloc", "Allocate memory", _alloc);
    kshell_register_command("free", "Free memory", _free);
}
