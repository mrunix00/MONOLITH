/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum : uint8_t {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEYBOARD,
    INPUT_EVENT_MOUSE,
} input_event_type_t;

typedef struct
{
    uint8_t scancode;
    uint8_t action;
} input_keyboard_event_t;

typedef struct
{
    int32_t x;
    int32_t y;
    int8_t delta_x;
    int8_t delta_y;
    uint8_t buttons;
} input_mouse_event_t;

typedef struct
{
    input_event_type_t type;
    uint8_t reserved;
    uint16_t reserved2;
    union {
        input_keyboard_event_t keyboard;
        input_mouse_event_t mouse;
    } data;
} input_event_t;

/* Keyboard scancodes - must match kernel's uint8_t enum */
typedef enum : uint8_t {
    KEY_BACKSPACE = 0x0E,
    KEY_TAB = 0x0F,
    KEY_RETURN = 0x1C,
    KEY_CTRL = 0x1D,
    KEY_LSHIFT = 0x2A,
    KEY_RSHIFT = 0x36,
    KEY_ALT = 0x38,
    KEY_CAPSLOCK = 0x3A,
    KEY_ESCAPE = 0x01,
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
    KEY_UP = 0x48,
    KEY_LEFT = 0x4B,
    KEY_DOWN = 0x50,
    KEY_RIGHT = 0x4D,
} keyboard_scancode_t;

/* Keyboard action - must match kernel's uint8_t enum */
typedef enum : uint8_t {
    KEYBOARD_HOLD = 0x00,
    KEYBOARD_PRESSED = 0x01,
    KEYBOARD_RELEASED = 0x02,
} keyboard_action_t;

/* Keyboard event structure - must match kernel layout (2 bytes total) */
typedef struct
{
    keyboard_scancode_t scancode;
    keyboard_action_t action;
} keyboard_event_t;

/* Initialize input system */
void input_init(void);

/* Get current mouse position */
int input_get_mouse_x(void);
int input_get_mouse_y(void);

/* Get mouse button state */
bool input_mouse_left(void);
bool input_mouse_right(void);
bool input_mouse_middle(void);

/* Check if left button was just pressed (single click detection) */
bool input_mouse_left_clicked(void);
bool input_mouse_right_clicked(void);

/* Get last keyboard character typed (ASCII, or 0 if none) */
char input_get_char(void);

/* Check if a key was pressed this frame */
bool input_key_pressed(keyboard_scancode_t key);

/* Update input state (call once per frame) */
void input_update(void);

/* Check if any input event occurred since last update */
bool input_has_events(void);

/* Clear the events flag (call after processing events) */
void input_clear_events(void);
