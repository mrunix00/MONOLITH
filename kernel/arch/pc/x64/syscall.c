/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/tasking/syscall.h>
#include <stdint.h>

extern void _syscall_handler();

#define IA32_EFER 0xC0000080
#define IA32_STAR 0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084
#define IA32_EFER_SCE (1ULL << 0)
#define RFLAGS_IF (1ULL << 9)
#define KERNEL_CODE_SELECTOR 0x08
#define USER_CODE_SELECTOR 0x1B

#define USER_SPACE_START 0x0000000000001000ULL
#define USER_SPACE_END 0x0000800000000000ULL

uintptr_t syscall_kernel_stack_top;

void syscalls_init()
{
    uint64_t efer = asm_read_msr(IA32_EFER);
    asm_write_msr(IA32_EFER, efer | IA32_EFER_SCE);
    asm_write_msr(
        IA32_STAR,
        ((uint64_t) (USER_CODE_SELECTOR & ~0x3) << 48) | ((uint64_t) KERNEL_CODE_SELECTOR << 32));
    asm_write_msr(IA32_LSTAR, (uint64_t) _syscall_handler);
    asm_write_msr(IA32_FMASK, RFLAGS_IF);
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
