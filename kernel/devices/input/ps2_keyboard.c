/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/interrupts.h>
#include <kernel/devices/input/input_device.h>
#include <kernel/devices/input/ps2_input.h>
#include <kernel/devices/input/ps2_keyboard.h>
#include <kernel/klibc/memory.h>

static input_keyboard_action_t _key_state[256];

#define IS_RELEASED(scancode) ((scancode) & 0x80)
#define GET_SCANCODE(event) ((event) & 0x7F)

static void _ps2_irq()
{
    uint8_t status = asm_inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OBF))
        return;
    if (status & PS2_STATUS_AUX)
        return;

    uint8_t ps2_event = asm_inb(PS2_DATA_PORT);
    uint8_t scancode = GET_SCANCODE(ps2_event);
    input_keyboard_action_t action;

    if (IS_RELEASED(ps2_event)) {
        _key_state[scancode] = INPUT_KEYBOARD_ACTION_RELEASED;
        action = INPUT_KEYBOARD_ACTION_RELEASED;
    } else if (
        _key_state[scancode] == INPUT_KEYBOARD_ACTION_PRESSED
        || _key_state[scancode] == INPUT_KEYBOARD_ACTION_HOLD) {
        _key_state[scancode] = INPUT_KEYBOARD_ACTION_HOLD;
        action = INPUT_KEYBOARD_ACTION_HOLD;
    } else {
        _key_state[scancode] = INPUT_KEYBOARD_ACTION_PRESSED;
        action = INPUT_KEYBOARD_ACTION_PRESSED;
    }

    input_push_keyboard_event(scancode, action);
}

void ps2_init_keyboard()
{
    ps2_write_command(PS2_CMD_DISABLE_KB);
    ps2_wait_obf_clear();

    uint8_t ccb = ps2_read_config();
    ccb |= PS2_CCB_KB_IRQ_ENABLE;
    ccb &= ~PS2_CCB_KB_CLOCK_OFF;
    ps2_write_config(ccb);

    ps2_write_command(PS2_CMD_ENABLE_KB);
    ps2_wait_obf_clear();
    memset(_key_state, INPUT_KEYBOARD_ACTION_RELEASED, sizeof(_key_state));
    irq_register_handler(1, _ps2_irq);
}
