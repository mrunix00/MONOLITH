/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

typedef struct
{
    uint16_t di, si, bp, sp, bx, dx, cx, ax, gs, fs, es, ds, eflags;
} __attribute__((packed)) int86_regs_t;

/*
 * A real mode far pointer.
 * https://en.wikipedia.org/wiki/Far_pointer#In_16-bit_x86
 */
typedef struct
{
    uint16_t offset;
    uint16_t segment;
} far_ptr_t;

/*
 * Calls a real mode interrupt from protected mode.
 */
void int86(int inum, int86_regs_t *regs);

/*
 * Converts a DOS far pointer to a physical address.
 */
static inline void *get_far_ptr(far_ptr_t ptr)
{
    return (void *) ((uintptr_t)(ptr.segment * 16 + ptr.offset));
}
