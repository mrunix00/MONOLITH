/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <shared/include/monolith/input_events.h>
#include <stdbool.h>
#include <stdint.h>

void input_push_keyboard_event(input_keyboard_scancode_t scancode, input_keyboard_action_t action);
void input_push_mouse_event(
    int32_t x, int32_t y, int8_t delta_x, int8_t delta_y, input_mouse_buttons_t buttons);
bool input_poll_keyboard_event(void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read);
bool input_poll_mouse_event(void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read);
void input_devices_init(void);
