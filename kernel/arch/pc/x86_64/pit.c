/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/pit.h>
#include <stdint.h>

#define PIT_CHANNEL0_PORT 0x40
#define PIT_CHANNEL1_PORT 0x41
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43

#define PIT_CLOCK_FREQUENCY 1193182

uint16_t pit_get_count()
{
    uint16_t count;

    asm_outb(0x0, PIT_COMMAND_PORT);          /* Channels in bits 6 and 7 */
    count = asm_inb(PIT_CHANNEL0_PORT);       /* Low byte */
    count |= asm_inb(PIT_CHANNEL0_PORT) << 8; /* High byte */

    return count;
}

void pit_set_reload(uint16_t count)
{
    asm_outb(count & 0xFF, PIT_CHANNEL0_PORT);          /* Low byte */
    asm_outb((count & 0xFF00) >> 8, PIT_CHANNEL0_PORT); /* High byte */
}

void pit_set_frequency(uint16_t frequency)
{
    uint16_t count = PIT_CLOCK_FREQUENCY / frequency;

    pit_set_reload(count);
}
