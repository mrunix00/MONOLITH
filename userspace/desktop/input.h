/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <input.h>
#include <libgfx/types.h>

void input_set_screen_bounds(uint32_t width, uint32_t height);

bool input_process_mouse_event(const input_mouse_event_t *event);

void draw_cursor(gfx_context_t *);

input_mouse_event_t get_mouse_state();
