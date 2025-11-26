/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

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
