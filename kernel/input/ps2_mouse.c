/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/input/input_events.h>
#include <kernel/input/ps2_mouse.h>
#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

uint8_t _mouse_cycle = 0;
uint8_t _mouse_byte[3];

/* Internal mouse state */
static int32_t _mouse_x = 0;
static int32_t _mouse_y = 0;
static bool _mouse_left_button = false;
static bool _mouse_right_button = false;
static bool _mouse_middle_button = false;
static int8_t _mouse_delta_x = 0;
static int8_t _mouse_delta_y = 0;

static void _mouse_irq()
{
    uint8_t status = asm_inb(PS2_STATUS_PORT);
    if (!(status & 0x01))
        return;

    /* Skip packet if it's not from mouse */
    if (!(status & 0x20)) {
        _mouse_cycle = 0;
        return;
    }

    switch (_mouse_cycle) {
    case 0:
        _mouse_byte[0] = asm_inb(PS2_DATA_PORT);
        _mouse_cycle++;
        break;
    case 1:
        _mouse_byte[1] = asm_inb(PS2_DATA_PORT);
        _mouse_cycle++;
        break;
    case 2:
        _mouse_byte[2] = asm_inb(PS2_DATA_PORT);
        _mouse_cycle = 0;

        /* Update button states */
        _mouse_left_button = _mouse_byte[0] & 0x01;
        _mouse_right_button = _mouse_byte[0] & 0x02;
        _mouse_middle_button = _mouse_byte[0] & 0x04;

        /* Calculate movement deltas with sign extension */
        int8_t delta_x = _mouse_byte[1];
        int8_t delta_y = _mouse_byte[2];

        /* Handle sign extension for 9-bit values */
        if (_mouse_byte[0] & 0x10) {
            delta_x |= 0xFFFFFF00; /* Sign extend */
        }
        if (_mouse_byte[0] & 0x20) {
            delta_y |= 0xFFFFFF00; /* Sign extend */
        }

        /* Update accumulated position */
        _mouse_x += delta_x;
        _mouse_y -= delta_y; /* Y is inverted */

        /* Store deltas for polling */
        _mouse_delta_x = delta_x;
        _mouse_delta_y = delta_y;

        uint8_t buttons = 0;
        if (_mouse_left_button)
            buttons |= 1 << 0;
        if (_mouse_right_button)
            buttons |= 1 << 1;
        if (_mouse_middle_button)
            buttons |= 1 << 2;
        input_push_mouse_event(_mouse_x, _mouse_y, delta_x, delta_y, buttons);
        break;
    }
}

static void _mouse_write(uint8_t cmd)
{
    asm_outb(0xD4, PS2_COMMAND_PORT);
    asm_outb(cmd, PS2_DATA_PORT);
}

static uint8_t _mouse_read()
{
    return asm_inb(PS2_DATA_PORT);
}

void ps2_mouse_init()
{
    /* Mouse initialization sequence */
    asm_outb(0xA8, PS2_COMMAND_PORT);
    asm_outb(0x20, PS2_COMMAND_PORT);

    uint8_t _status = (asm_inb(PS2_DATA_PORT) | 2);
    asm_outb(0x60, PS2_COMMAND_PORT);
    asm_outb(_status, PS2_DATA_PORT);

    _mouse_write(0xF6);
    _mouse_read();
    _mouse_write(0xF4);
    _mouse_read();

    /* Initialize mouse state */
    _mouse_x = 0;
    _mouse_y = 0;
    _mouse_left_button = false;
    _mouse_right_button = false;
    _mouse_middle_button = false;
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;

    irq_register_handler(12, _mouse_irq);
}

void ps2_mouse_get_state(mouse_state_t *state)
{
    if (!state)
        return;

    state->x = _mouse_x;
    state->y = _mouse_y;
    state->left_button = _mouse_left_button;
    state->right_button = _mouse_right_button;
    state->middle_button = _mouse_middle_button;
    state->delta_x = _mouse_delta_x;
    state->delta_y = _mouse_delta_y;

    /* Clear deltas after reading */
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;
}
