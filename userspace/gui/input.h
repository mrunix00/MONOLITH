/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    struct
    {
        bool left_button : 1;
        bool right_button : 1;
        bool middle_button : 1;
        bool always_on : 1;
        bool x_sign : 1;
        bool y_sign : 1;
        bool x_overflow : 1;
        bool y_overflow : 1;
    };
    uint8_t x_movement;
    uint8_t y_movement;
} mouse_event_t;

typedef enum : uint8_t {
    KEY_BACKSPACE = 0x0E,
    KEY_LSHIFT = 0x2A,
    KEY_RSHIFT = 0x36,
    KEY_RETURN = 0x1C,
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

typedef void (*mouse_event_handler_t)(mouse_event_t);
typedef void (*keyboard_event_handler_t)(keyboard_event_t);

int register_mouse_event_handler(mouse_event_handler_t);
int register_keyboard_event_handler(keyboard_event_handler_t);
void init_input();
void draw_cursor();
