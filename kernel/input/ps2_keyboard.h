/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/usermode/task.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum : uint8_t {
    KEY_LSHIFT = 0x2A,
    KEY_RSHIFT = 0x36,
    KEY_CTRL = 0x1D,
    KEY_ALT = 0x38,
    KEY_CAPSLOCK = 0x3A,
    KEY_NUMLOCK = 0x45,
    KEY_SCROLLLOCK = 0x46,
    KEY_F1 = 0x3B,
    KEY_F2 = 0x3C,
    KEY_F3 = 0x3D,
    KEY_F4 = 0x3E,
    KEY_F5 = 0x3F,
    KEY_F6 = 0x40,
    KEY_F7 = 0x41,
    KEY_F8 = 0x42,
    KEY_F9 = 0x43,
    KEY_F10 = 0x44,
    KEY_F11 = 0x57,
    KEY_F12 = 0x58,
    KEY_PRINTSCREEN = 0x37,
    KEY_INSERT = 0x52,
    KEY_HOME = 0x47,
    KEY_PAGEUP = 0x49,
    KEY_DELETE = 0x53,
    KEY_END = 0x4F,
    KEY_PAGEDOWN = 0x51,
    KEY_UP = 0x48,
    KEY_LEFT = 0x4B,
    KEY_DOWN = 0x50,
    KEY_RIGHT = 0x4D,
} keyboard_scancode_t;

typedef enum : uint8_t {
    KEYBOARD_HOLD = 0x00,
    KEYBOARD_PRESSED = 0x01,
    KEYBOARD_RELEASED = 0x02,
} keyboard_action_t;

typedef struct
{
    keyboard_scancode_t scancode;
    keyboard_action_t action;
} keyboard_event_t;

typedef struct
{
    const char *name;
    const char keymap[0x57];
    const char shifted_keymap[0x57];
} keyboard_layout_t;

enum {
    KB_LAYOUT_US = 0,
};

static const keyboard_layout_t keyboard_layouts[] = {
    [KB_LAYOUT_US] = {
        "US",
        {0x0,  0x0, '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  '\b',
         '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  '[', ']', '\n', 0x0,
         'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x0, '\\', 'z',
         'x',  'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0x0, '*',  0x0, ' ', 0x0,  0x0,
         0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  '7', '8', '9',  '-',
         '4',  '5', '6', '+', '1', '2', '3', '0', '.', 0x0, 0x0,  0x0},
        {0x0,  0x0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  '\b',
         '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0x0,
         'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0x0, '|',  'Z',
         'X',  'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0x0, '*', 0x0, ' ', 0x0,  0x0,
         0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  '-',
         0x0,  0x0, 0x0, '+', 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
    },
};

typedef void (*keyboard_event_handler_t)(keyboard_event_t);

void ps2_init_keyboard();
int ps2_keyboard_register_event_handler(keyboard_event_handler_t handler);
void ps2_keyboard_unregister_handlers_for_task(task_t *task);
