/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/pic.h>
#include <kernel/debug.h>

static bool _pic_initialized = false;

void pic_init()
{
    debug_log("Initializing the PIC\n");

    /* Remap the PIC */
    asm_outb(0x11, 0x20);
    asm_outb(0x11, 0xA0);
    asm_outb(0x20, 0x21);
    asm_outb(0x28, 0xA1);
    asm_outb(0x04, 0x21);
    asm_outb(0x02, 0xA1);
    asm_outb(0x01, 0x21);
    asm_outb(0x01, 0xA1);
    asm_outb(0x00, 0x21);
    asm_outb(0x00, 0xA1);

    _pic_initialized = true;
    debug_log("PIC initialized\n");
}

void pic_disable()
{
    asm_outb(0xFF, 0x21);
    asm_outb(0xFF, 0xA1);

    debug_log("PIC disabled\n");
    _pic_initialized = false;
}

void pic_eoi(uint8_t isr)
{
    if (isr >= 40)
        asm_outb(0x20, 0xA0);
    asm_outb(0x20, 0x20);
}

void pic_set_irq_mask(uint8_t irq, bool mask)
{
    if (irq >= 8)
        asm_outb(mask ? 0xFF : 0x00, 0xA1);
    else
        asm_outb(mask ? 0xFF : 0x00, 0x21);
}

bool pic_is_enabled()
{
    return _pic_initialized;
}
