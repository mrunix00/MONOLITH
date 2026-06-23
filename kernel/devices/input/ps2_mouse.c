/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/interrupts.h>
#include <kernel/devices/input/input_device.h>
#include <kernel/devices/input/ps2_input.h>
#include <kernel/devices/input/ps2_mouse.h>

uint8_t _mouse_cycle = 0;
uint8_t _mouse_byte[3];

#define MOUSE_STATUS_SYNC 0x08
#define MOUSE_STATUS_LEFT_BUTTON 0x01
#define MOUSE_STATUS_RIGHT_BUTTON 0x02
#define MOUSE_STATUS_MIDDLE_BUTTON 0x04
#define MOUSE_STATUS_OVERFLOW 0xC0

/* Internal mouse state */
static int32_t _mouse_x = 0;
static int32_t _mouse_y = 0;
static bool _mouse_left_button = false;
static bool _mouse_right_button = false;
static bool _mouse_middle_button = false;

static void _mouse_irq()
{
    uint8_t status = asm_inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OBF))
        return;

    if (!(status & PS2_STATUS_AUX)) {
        _mouse_cycle = 0;
        return;
    }

    switch (_mouse_cycle) {
    case 0:
        _mouse_byte[0] = asm_inb(PS2_DATA_PORT);
        /* Sync packet: bit 3 must always be set */
        if (!(_mouse_byte[0] & MOUSE_STATUS_SYNC)) {
            _mouse_cycle = 0;
            break;
        }
        _mouse_cycle++;
        break;
    case 1:
        _mouse_byte[1] = asm_inb(PS2_DATA_PORT);
        _mouse_cycle++;
        break;
    case 2:
        _mouse_byte[2] = asm_inb(PS2_DATA_PORT);
        _mouse_cycle = 0;

        /* Drop packet on overflow */
        if (_mouse_byte[0] & MOUSE_STATUS_OVERFLOW)
            break;

        int32_t delta_x = (int8_t) _mouse_byte[1];
        int32_t delta_y = (int8_t) _mouse_byte[2];
        _mouse_x += delta_x;
        _mouse_y -= delta_y;
        _mouse_left_button = _mouse_byte[0] & MOUSE_STATUS_LEFT_BUTTON;
        _mouse_right_button = _mouse_byte[0] & MOUSE_STATUS_RIGHT_BUTTON;
        _mouse_middle_button = _mouse_byte[0] & MOUSE_STATUS_MIDDLE_BUTTON;
        uint8_t buttons = _mouse_left_button | _mouse_right_button << 1 | _mouse_middle_button << 2;
        input_push_mouse_event(_mouse_x, _mouse_y, delta_x, delta_y, buttons);
        break;
    }
}

static void _mouse_write(uint8_t cmd)
{
    ps2_write_command(PS2_CMD_WRITE_AUX);
    ps2_write(cmd);
}

void ps2_mouse_init()
{
    interrupts_disable();

    /* Mouse initialization sequence */
    ps2_wait_obf_clear();
    ps2_write_command(PS2_CMD_ENABLE_AUX);

    uint8_t ccb = ps2_read_config();
    ccb |= PS2_CCB_AUX_IRQ_ENABLE;
    ccb &= ~PS2_CCB_AUX_CLOCK_OFF;
    ps2_write_config(ccb);

    _mouse_write(0xF6);
    ps2_read();
    _mouse_write(0xF4);
    ps2_read();

    /* Initialize mouse state */
    _mouse_x = 0;
    _mouse_y = 0;
    _mouse_left_button = false;
    _mouse_right_button = false;
    _mouse_middle_button = false;
    _mouse_cycle = 0;

    irq_register_handler(12, _mouse_irq);
    interrupts_enable();
}
