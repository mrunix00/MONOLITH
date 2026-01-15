/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./input.h"
#include "./cursor.h"

#include <libgfx.h>

static input_mouse_event_t mouse_state = (input_mouse_event_t) {
    .x = 0,
    .y = 0,
    .delta_x = 0,
    .delta_y = 0,
    .buttons = 0,
};

bool handle_input(gfx_context_t *context)
{
    input_event_t event;
    bool had_event = false;
    while (poll_input_event(&event) == 0) {
        had_event = true;
        if (event.type == INPUT_EVENT_MOUSE) {
            uint32_t new_x = event.data.mouse.x < 0 ? 0u : (uint32_t) event.data.mouse.x;
            uint32_t new_y = event.data.mouse.y < 0 ? 0u : (uint32_t) event.data.mouse.y;

            if (new_x >= context->width)
                new_x = (uint32_t) context->width - 1;
            if (new_y >= context->height)
                new_y = (uint32_t) context->height - 1;

            mouse_state = event.data.mouse;
            mouse_state.x = (int32_t) new_x;
            mouse_state.y = (int32_t) new_y;
        }
    }

    return had_event;
}

void draw_cursor(gfx_context_t *context)
{
    gfx_draw_colored_bitmap(
        context,
        &_cursor_bitmap,
        (gfx_pos_t) {mouse_state.x, mouse_state.y},
        (gfx_color_t) {0xff, 0xff, 0xff, 0xff});
}

input_mouse_event_t get_mouse_state()
{
    return mouse_state;
}
