/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/input/ps2_keyboard.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

static keyboard_action_t _key_state[256];
static uint8_t _led_state = 0;

typedef struct
{
    keyboard_event_handler_t handler;
    task_t *owner;
} keyboard_handler_entry_t;

static keyboard_handler_entry_t *_event_handlers = NULL;
static uint16_t _event_handler_count = 0;
static uint16_t _event_handler_capacity = 16;

#define IS_RELEASED(scancode) ((scancode) & 0x80)
#define GET_SCANCODE(event) ((event) & 0x7F)

static void _ps2_irq()
{
    uint8_t status;
    while ((status = asm_inb(PS2_STATUS_PORT)) & 0x01) {
        /* Skip if the event is from the mouse */
        if (status & 0x20)
            return;

        uint8_t ps2_event = asm_inb(PS2_DATA_PORT);
        uint8_t scancode = GET_SCANCODE(ps2_event);
        keyboard_action_t action;

        if (IS_RELEASED(ps2_event)) {
            _key_state[scancode] = KEYBOARD_RELEASED;
            action = KEYBOARD_RELEASED;
        } else if (_key_state[scancode] == KEYBOARD_PRESSED || _key_state[scancode] == KEYBOARD_HOLD) {
            _key_state[scancode] = KEYBOARD_HOLD;
            action = KEYBOARD_HOLD;
        } else {
            _key_state[scancode] = KEYBOARD_PRESSED;
            action = KEYBOARD_PRESSED;
        }

        if (action == KEYBOARD_PRESSED && scancode == KEY_CAPSLOCK) {
            /* Wait until input buffer is empty */
            while (asm_inb(PS2_STATUS_PORT) & 0x02)
                ;

            asm_outb(0xED, PS2_DATA_PORT); /* Set LED command */

            /* Wait for ACK */
            while ((asm_inb(PS2_STATUS_PORT) & 0x01) == 0)
                ;
            uint8_t ack = asm_inb(PS2_DATA_PORT);
            if (ack != 0xFA)
                continue;

            /* Wait until input buffer is empty again */
            while (asm_inb(PS2_STATUS_PORT) & 0x02)
                ;

            _led_state ^= 0x04;
            asm_outb(_led_state, PS2_DATA_PORT);
        }

        keyboard_event_t event = {
            .scancode = scancode,
            .action = action,
        };
        for (int i = 0; i < _event_handler_capacity; i++) {
            if (_event_handlers[i].handler != NULL)
                _event_handlers[i].handler(event);
        }
    }
}

void ps2_init_keyboard()
{
    while (asm_inb(PS2_STATUS_PORT) & 0x01)
        asm_inb(PS2_DATA_PORT);
    irq_register_handler(1, _ps2_irq);
    _event_handlers = kmalloc(_event_handler_capacity * sizeof(keyboard_handler_entry_t));
    memset(_event_handlers, 0, _event_handler_capacity * sizeof(keyboard_handler_entry_t));
    memset(_key_state, KEYBOARD_RELEASED, sizeof(_key_state));
}

int ps2_keyboard_register_event_handler(keyboard_event_handler_t handler)
{
    task_t *owner = task_get_current();
    if (_event_handler_count + 1 == _event_handler_capacity) {
        keyboard_handler_entry_t *new_ptr = krealloc(
            _event_handlers,
            _event_handler_capacity * 2 * sizeof(keyboard_handler_entry_t));
        if (new_ptr == NULL)
            return -1;
        memset(
            new_ptr + _event_handler_capacity,
            0,
            _event_handler_capacity * sizeof(keyboard_handler_entry_t));
        _event_handler_capacity *= 2;
        _event_handlers = new_ptr;
    }

    for (int i = 0; i < _event_handler_capacity; i++) {
        if (_event_handlers[i].handler == NULL) {
            _event_handlers[i].handler = handler;
            _event_handlers[i].owner = owner;
            _event_handler_count++;
            return 0;
        }
    }

    return -1;
}

void ps2_keyboard_unregister_handlers_for_task(task_t *task)
{
    if (!task || !_event_handlers)
        return;

    for (int i = 0; i < _event_handler_capacity; i++) {
        if (_event_handlers[i].handler != NULL && _event_handlers[i].owner == task) {
            _event_handlers[i].handler = NULL;
            _event_handlers[i].owner = NULL;
            if (_event_handler_count > 0)
                _event_handler_count--;
        }
    }
}
