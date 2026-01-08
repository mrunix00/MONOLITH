/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
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
    uint8_t buttons; /* bit0=left, bit1=right, bit2=middle */
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

void input_events_init(void);
void input_push_keyboard_event(uint8_t scancode, uint8_t action);
void input_push_mouse_event(int32_t x, int32_t y, int8_t delta_x, int8_t delta_y, uint8_t buttons);
bool input_poll_event(input_event_t *event);
