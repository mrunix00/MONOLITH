/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

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
typedef struct __attribute__((packed))
{
#if defined(__x86_64__)
    gdt_entry_t entries[5];
    gdt_tss_entry_t tss;
#elif defined(__i386__)
    gdt_entry_t entries[6];
#endif
} gdt_t;

extern gdt_t gdt;

/*
 * GDTR structure.
 * https://wiki.osdev.org/Global_Descriptor_Table#GDTR
 */
typedef struct
{
    uint16_t limit;
#if defined(__x86_64__)
    uint64_t base;
#elif defined(__i386__)
    uint32_t base;
#endif
} __attribute__((packed)) gdtr_t;

/*
 * Task State Segment structure.
 * https://wiki.osdev.org/Task_State_Segment
 */

typedef struct
#if defined(__x86_64__)
{
    uint32_t reserved;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
#elif defined(__i386__)
{
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
#endif
} __attribute__((packed)) tss_entry_t;

/*
 * Initialize the Global Descriptor Table.
 * https://wiki.osdev.org/Global_Descriptor_Table
 */
void gdt_init();

/*
 * Set the value of a GDT gate.
 * https://wiki.osdev.org/Global_Descriptor_Table
 */
void gdt_set_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

/*
 * Load the Task State Segment.
 * https://wiki.osdev.org/Task_State_Segment
 */
void gdt_tss_load(void *);

/*
 * Flush the GDT.
 * https://wiki.osdev.org/Global_Descriptor_Table#Loading_the_GDT
 */
void gdt_flush();

/*
 * Flush the TSS.
 * https://wiki.osdev.org/Task_State_Segment#Loading_the_TSS
 */
void gdt_flush_tss();

/*
 * Set the RSP0 value in the TSS.
 */
void gdt_tss_set_rsp0(uint64_t rsp0);
