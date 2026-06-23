/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <resource.h>
#include <stdint.h>

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

static inline rsrc_handle_t open_keyboard_device(void)
{
    return rsmgr_open("device:/input/keyboard");
}

static inline rsrc_handle_t open_mouse_device(void)
{
    return rsmgr_open("device:/input/mouse");
}

static inline int read_keyboard_event(rsrc_handle_t fd, input_keyboard_event_t *event)
{
    return rsmgr_read(fd, event, sizeof(input_keyboard_event_t));
}

static inline int read_mouse_event(rsrc_handle_t fd, input_mouse_event_t *event)
{
    return rsmgr_read(fd, event, sizeof(input_mouse_event_t));
}
