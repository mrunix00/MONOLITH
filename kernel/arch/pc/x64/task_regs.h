/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

typedef struct
{
    uintptr_t cr3;
    uintptr_t rip;
    uintptr_t rsp;
    uintptr_t rsp0;
    uintptr_t rflags;
    uintptr_t rax;
    uintptr_t rbx;
    uintptr_t rcx;
    uintptr_t rdx;
    uintptr_t rsi;
    uintptr_t rdi;
    uintptr_t rbp;
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uint16_t cs;
    uint16_t ss;
    void *fx_state;
    void *fx_state_aligned;
} task_regs_t;
