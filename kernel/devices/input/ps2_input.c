/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/interrupts.h>
#include <kernel/devices/input/ps2_input.h>

void ps2_wait_ibf_clear()
{
    while ((asm_inb(PS2_STATUS_PORT) & PS2_STATUS_IBF) != 0)
        asm_pause();
}

void ps2_wait_obf_set()
{
    while ((asm_inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) == 0)
        asm_pause();
}

void ps2_wait_obf_clear()
{
    while ((asm_inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) != 0)
        asm_inb(PS2_DATA_PORT);
}

void ps2_write_command(uint8_t cmd)
{
    ps2_wait_ibf_clear();
    asm_outb(cmd, PS2_COMMAND_PORT);
}

void ps2_write(uint8_t value)
{
    ps2_wait_ibf_clear();
    asm_outb(value, PS2_DATA_PORT);
}

uint8_t ps2_read()
{
    ps2_wait_obf_set();
    return asm_inb(PS2_DATA_PORT);
}

uint8_t ps2_read_config()
{
    interrupts_disable();
    ps2_write_command(PS2_CMD_READ_CCB);
    uint8_t ccb = ps2_read();
    interrupts_enable();
    return ccb;
}

void ps2_write_config(uint8_t ccb)
{
    interrupts_disable();
    ps2_write_command(PS2_CMD_WRITE_CCB);
    ps2_write(ccb);
    interrupts_enable();
}
