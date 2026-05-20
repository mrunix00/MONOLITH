/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/apic.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/interrupts.h>
#include <kernel/arch/pc/pic.h>
#include <stdint.h>

void interrupts_eoi(uint8_t isr)
{
    if (pic_is_enabled())
        pic_eoi(isr);
    else if (apic_is_initialized())
        apic_eoi();
}

void interrupts_set_irq_mask(uint8_t irq, bool mask)
{
    if (pic_is_enabled())
        pic_set_irq_mask(irq, mask);
    else if (apic_is_initialized())
        apic_set_irq_mask(irq, mask);
}

static uint64_t _interrupt_disable_count = 0;
static bool _first_interrupt_enable = true;
void interrupts_disable()
{
    if (_interrupt_disable_count == 0)
        asm_cli();
    _interrupt_disable_count++;
}

void interrupts_enable()
{
    if (_interrupt_disable_count == 0) {
        if (_first_interrupt_enable) {
            asm_sti();
            _first_interrupt_enable = false;
        }
        return;
    }
    _interrupt_disable_count--;
    if (_interrupt_disable_count == 0)
        asm_sti();
}
