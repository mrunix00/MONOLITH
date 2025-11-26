/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

#define IDT_TYPE_INTERRUPT 0x8E
#define IDT_TYPE_SOFTWARE 0xEF

typedef struct interrupt_registers
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t core;
    uint64_t isr_number;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_registers_t;

/*
 * Initialize the Interrupt Descriptor Table
 * https://wiki.osdev.org/Interrupt_Descriptor_Table
 */
void idt_init(void);

/*
 * Set the IDT gate
 * https://wiki.osdev.org/Interrupts_Tutorial#Assembling
 */
void idt_set_gate(uint8_t num, void *handler, uint8_t flags);

/*
 * Flush the IDT
 * https://wiki.osdev.org/Interrupt_Descriptor_Table
 */
void idt_flush();

/*
 * Interrupt Service Routine Handler
 * https://wiki.osdev.org/Interrupt_Service_Routines
 */
void isr_handler(struct interrupt_registers *);

/*
 * Interrupt Service Routine Handler
 * https://wiki.osdev.org/Interrupt_Service_Routines
 */
void irq_handler(struct interrupt_registers *);

/*
 * Register an IRQ Service Routine
 * https://wiki.osdev.org/Interrupts
 */
void irq_register_handler(uint8_t num, void *handler);

/*
 * Unregister IRQ Service Routine
 * https://wiki.osdev.org/Interrupts
 */
void irq_unregister_handler(uint8_t num);
