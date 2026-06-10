/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/gdt.h>
#include <kernel/debug.h>

gdt_t gdt = {0};
gdtr_t gdtr = {0};

static tss_entry_t _tss = {0};
extern uintptr_t syscall_kernel_stack_top;

void gdt_init()
{
    debug_log("Initializing the GDT...\n");

    /* https://wiki.osdev.org/GDT_Tutorial#Flat_/_Long_Mode_Setup */
    gdt_set_gate(0, 0, 0, 0, 0);                /* Null segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Code segment */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Data segment */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User mode code segment */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User mode data segment */

    /* TSS */
    _tss.ss0 = 0x10;
    _tss.esp0 = 0x9FF00;
    _tss.iomap_base = sizeof(_tss);
    syscall_kernel_stack_top = _tss.esp0;
    gdt_tss_load(&_tss);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint32_t) (uintptr_t) &gdt;

    debug_log("Flushing the GDT...\n");
    gdt_flush();
    debug_log("Flushing the TSS...\n");
    gdt_flush_tss();
    debug_log("Initialized the GDT\n");
}

void gdt_tss_set_rsp0(uint64_t rsp0)
{
    _tss.esp0 = (uint32_t) rsp0;
    syscall_kernel_stack_top = (uintptr_t) rsp0;
}

void gdt_set_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt.entries[index].base_low = base & 0xFFFF;
    gdt.entries[index].base_middle = (base >> 16) & 0xFF;
    gdt.entries[index].base_high = (base >> 24) & 0xFF;
    gdt.entries[index].limit_low = limit & 0xFFFF;
    gdt.entries[index].granularity = (limit >> 16) & 0x0F;
    gdt.entries[index].granularity |= gran & 0xF0;
    gdt.entries[index].access = access;
}

void gdt_tss_load(void *tss)
{
    uint32_t base = (uint32_t) (uintptr_t) tss;
    uint32_t limit = sizeof(tss_entry_t) - 1;
    gdt_set_gate(5, base, limit, 0x89, 0x40);
}
