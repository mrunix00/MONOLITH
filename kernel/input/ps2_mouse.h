/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t x;
    int32_t y;
    bool left_button;
    bool right_button;
    bool middle_button;
    int8_t delta_x;
    int8_t delta_y;
} mouse_state_t;

void ps2_mouse_init();
void ps2_mouse_get_state(mouse_state_t *state);
