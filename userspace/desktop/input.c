/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./input.h"
#include "./cursor.h"

#include <libgfx.h>

static input_mouse_event_t _mouse_state
    = (input_mouse_event_t) {.x = 0, .y = 0, .delta_x = 0, .delta_y = 0, .buttons = 0};
static uint32_t _screen_width = 0;
static uint32_t _screen_height = 0;

void input_set_screen_bounds(uint32_t width, uint32_t height)
{
    _screen_width = width;
    _screen_height = height;
}

bool input_process_mouse_event(const input_mouse_event_t *event)
{
    if (!event)
        return false;

    uint32_t new_x = event->x < 0 ? 0u : (uint32_t) event->x;
    uint32_t new_y = event->y < 0 ? 0u : (uint32_t) event->y;

    if (_screen_width > 0 && new_x >= _screen_width)
        new_x = _screen_width - 1;
    if (_screen_height > 0 && new_y >= _screen_height)
        new_y = _screen_height - 1;

    _mouse_state = *event;
    _mouse_state.x = (int32_t) new_x;
    _mouse_state.y = (int32_t) new_y;
    return true;
}

void draw_cursor(gfx_context_t *context)
{
    gfx_draw_colored_bitmap(context, &_cursor_bitmap, (gfx_pos_t) {_mouse_state.x, _mouse_state.y});
}

input_mouse_event_t get_mouse_state()
{
    return _mouse_state;
}
