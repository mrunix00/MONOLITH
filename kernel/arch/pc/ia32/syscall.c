/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/idt.h>
#include <kernel/tasking/syscall.h>
#include <stdint.h>

extern void _syscall_handler();

#define USER_SPACE_START 0x00001000UL
#define USER_SPACE_END 0xBFF00000UL

uintptr_t syscall_kernel_stack_top;

void syscalls_init()
{
    idt_set_gate(0x80, (void *) _syscall_handler, IDT_TYPE_SOFTWARE);
}

bool syscall_user_ptr_range(const void *ptr, size_t size)
{
    if (!ptr)
        return false;

    uintptr_t start = (uintptr_t) ptr;
    if (start < USER_SPACE_START || start >= USER_SPACE_END)
        return false;

    if (size == 0)
        return true;

    uintptr_t end = start + size;
    if (end < start)
        return false;

    return end <= USER_SPACE_END;
}
