/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

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

/*
 * Poll for input events. Returns 0 on event, 1 if none.
 */
static inline int poll_input_event(input_event_t *event)
{
    return syscall1(SYSCALL_POLL_INPUT_EVENT, (long) event);
}
