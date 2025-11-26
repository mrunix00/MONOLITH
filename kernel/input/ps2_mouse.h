/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/usermode/task.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    struct
    {
        bool left_button : 1;
        bool right_button : 1;
        bool middle_button : 1;
        bool always_on : 1;
        bool x_sign : 1;
        bool y_sign : 1;
        bool x_overflow : 1;
        bool y_overflow : 1;
    };
    uint8_t x_movement;
    uint8_t y_movement;
} mouse_event_t;

typedef void (*ps2_mouse_event_handler_t)(mouse_event_t);

void ps2_mouse_init();
int ps2_mouse_register_event_handler(ps2_mouse_event_handler_t handler);
void ps2_mouse_unregister_handlers_for_task(task_t *task);
