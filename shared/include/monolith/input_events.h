/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

typedef uint8_t input_keyboard_scancode_t;
typedef uint8_t input_keyboard_action_t;
typedef uint8_t input_mouse_buttons_t;

#define INPUT_KEY_NULL 0x00
#define INPUT_KEY_ESCAPE 0x01
#define INPUT_KEY_1 0x02
#define INPUT_KEY_2 0x03
#define INPUT_KEY_3 0x04
#define INPUT_KEY_4 0x05
#define INPUT_KEY_5 0x06
#define INPUT_KEY_6 0x07
#define INPUT_KEY_7 0x08
#define INPUT_KEY_8 0x09
#define INPUT_KEY_9 0x0A
#define INPUT_KEY_0 0x0B
#define INPUT_KEY_MINUS 0x0C
#define INPUT_KEY_EQUALS 0x0D
#define INPUT_KEY_BACKSPACE 0x0E
#define INPUT_KEY_TAB 0x0F
#define INPUT_KEY_Q 0x10
#define INPUT_KEY_W 0x11
#define INPUT_KEY_E 0x12
#define INPUT_KEY_R 0x13
#define INPUT_KEY_T 0x14
#define INPUT_KEY_Y 0x15
#define INPUT_KEY_U 0x16
#define INPUT_KEY_I 0x17
#define INPUT_KEY_O 0x18
#define INPUT_KEY_P 0x19
#define INPUT_KEY_LBRACKET 0x1A
#define INPUT_KEY_RBRACKET 0x1B
#define INPUT_KEY_ENTER 0x1C
#define INPUT_KEY_LCTRL 0x1D
#define INPUT_KEY_A 0x1E
#define INPUT_KEY_S 0x1F
#define INPUT_KEY_D 0x20
#define INPUT_KEY_F 0x21
#define INPUT_KEY_G 0x22
#define INPUT_KEY_H 0x23
#define INPUT_KEY_J 0x24
#define INPUT_KEY_K 0x25
#define INPUT_KEY_L 0x26
#define INPUT_KEY_SEMICOLON 0x27
#define INPUT_KEY_APOSTROPHE 0x28
#define INPUT_KEY_BACKTICK 0x29
#define INPUT_KEY_LSHIFT 0x2A
#define INPUT_KEY_BACKSLASH 0x2B
#define INPUT_KEY_Z 0x2C
#define INPUT_KEY_X 0x2D
#define INPUT_KEY_C 0x2E
#define INPUT_KEY_V 0x2F
#define INPUT_KEY_B 0x30
#define INPUT_KEY_N 0x31
#define INPUT_KEY_M 0x32
#define INPUT_KEY_COMMA 0x33
#define INPUT_KEY_PERIOD 0x34
#define INPUT_KEY_SLASH 0x35
#define INPUT_KEY_RSHIFT 0x36
#define INPUT_KEY_KEYPAD_ASTERISK 0x37
#define INPUT_KEY_LALT 0x38
#define INPUT_KEY_SPACE 0x39
#define INPUT_KEY_CAPSLOCK 0x3A
#define INPUT_KEY_F1 0x3B
#define INPUT_KEY_F2 0x3C
#define INPUT_KEY_F3 0x3D
#define INPUT_KEY_F4 0x3E
#define INPUT_KEY_F5 0x3F
#define INPUT_KEY_F6 0x40
#define INPUT_KEY_F7 0x41
#define INPUT_KEY_F8 0x42
#define INPUT_KEY_F9 0x43
#define INPUT_KEY_F10 0x44
#define INPUT_KEY_NUMLOCK 0x45
#define INPUT_KEY_SCROLLLOCK 0x46
#define INPUT_KEY_HOME 0x47
#define INPUT_KEY_UP 0x48
#define INPUT_KEY_PAGEUP 0x49
#define INPUT_KEY_LEFT 0x4B
#define INPUT_KEY_RIGHT 0x4D
#define INPUT_KEY_END 0x4F
#define INPUT_KEY_DOWN 0x50
#define INPUT_KEY_PAGEDOWN 0x51
#define INPUT_KEY_INSERT 0x52
#define INPUT_KEY_DELETE 0x53
#define INPUT_KEY_F11 0x57
#define INPUT_KEY_F12 0x58

#define INPUT_KEYBOARD_ACTION_HOLD 0
#define INPUT_KEYBOARD_ACTION_PRESSED 1
#define INPUT_KEYBOARD_ACTION_RELEASED 2

#define INPUT_MOUSE_BUTTON_LEFT 0x01
#define INPUT_MOUSE_BUTTON_RIGHT 0x02
#define INPUT_MOUSE_BUTTON_MIDDLE 0x04

typedef struct
{
    input_keyboard_scancode_t scancode;
    input_keyboard_action_t action;
} input_keyboard_event_t;

typedef struct
{
    int32_t x;
    int32_t y;
    int8_t delta_x;
    int8_t delta_y;
    input_mouse_buttons_t buttons;
} input_mouse_event_t;
