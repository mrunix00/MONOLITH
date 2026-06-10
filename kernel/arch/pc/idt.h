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
    uintptr_t r15;
    uintptr_t r14;
    uintptr_t r13;
    uintptr_t r12;
    uintptr_t r11;
    uintptr_t r10;
    uintptr_t r9;
    uintptr_t r8;
    uintptr_t rsi;
    uintptr_t rdi;
    uintptr_t rbp;
    uintptr_t rdx;
    uintptr_t rcx;
    uintptr_t rbx;
    uintptr_t rax;
    uintptr_t core;
    uintptr_t isr_number;
    uintptr_t error_code;
    uintptr_t rip;
    uintptr_t cs;
    uintptr_t rflags;
    uintptr_t rsp;
    uintptr_t ss;
} __attribute__((packed)) interrupt_registers_t;

void idt_init(void);
void idt_set_gate(uint8_t num, void *handler, uint8_t flags);
void idt_flush();
void isr_handler(struct interrupt_registers *);
void irq_handler(struct interrupt_registers *);
