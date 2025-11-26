/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/x86_64/tss.h>
#include <kernel/debug.h>

/*
 * Global Descriptor Table Entry Descriptor.
 * https://wiki.osdev.org/Global_Descriptor_Table#Long_Mode_System_Segment_Descriptor
 */
typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_entry_t;

/*
 * Global Descriptor Table.
 * https://wiki.osdev.org/Global_Descriptor_Table#Table
 */
struct
{
    gdt_entry_t entries[5];
    gdt_tss_entry_t tss;
} __attribute__((packed)) gdt;

/*
 * GDTR structure.
 * https://wiki.osdev.org/Global_Descriptor_Table#GDTR
 */
struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr = {0};

static tss_entry_t _tss = {0};

void gdt_init()
{
    debug_log("[*] Initializing the GDT...\n");

    /* https://wiki.osdev.org/GDT_Tutorial#Flat_/_Long_Mode_Setup */
    gdt_set_gate(0, 0, 0, 0, 0);                /* Null segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0x20); /* Code segment */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xA0); /* Data segment */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0x20); /* User mode code segment */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xA0); /* User mode data segment */

    /* TSS */
    _tss.iomap_base = sizeof(_tss);
    _tss.rsp0 = 0xFFFFFFFFFFFFF000LL; /* Kernel stack pointer */
    gdt_tss_load(&_tss);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t) &gdt;

    debug_log("[*] Flushing the GDT...\n");
    gdt_flush();
    debug_log("[*] Flushing the TSS...\n");
    gdt_flush_tss();
    debug_log("[+] GDT initialized\n");
}

void gdt_set_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt.entries[index].base_low = (base & 0xFFFF);
    gdt.entries[index].base_middle = (base >> 16) & 0xFF;
    gdt.entries[index].base_high = (base >> 24) & 0xFF;

    gdt.entries[index].limit_low = (limit & 0xFFFF);
    gdt.entries[index].granularity = (limit >> 16) & 0x0F;

    gdt.entries[index].granularity |= gran & 0xF0;
    gdt.entries[index].access = access;
}

void gdt_tss_load(void *tss)
{
    const uint64_t addr = (uint64_t) tss;
    gdt.tss.limit_low = sizeof(tss_entry_t) - 1;
    gdt.tss.base_low = addr & 0xFFFF;
    gdt.tss.base_middle = (addr >> 16) & 0xFF;
    gdt.tss.base_high = (addr >> 24) & 0xFF;
    gdt.tss.base_upper32 = addr >> 32;
    gdt.tss.access = 0x89;
    gdt.tss.granularity = 0x00;
    gdt.tss.reserved = 0;
}
