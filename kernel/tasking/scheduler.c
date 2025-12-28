/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/task.h>
#include <stdbool.h>

static bool _initialized = false;

void scheduler_init()
{
    _initialized = true;
}

void scheduler_tick()
{
    if (!_initialized)
        return;

    task_t *current = task_get_current();
    if (current->quantum_remaining == 0) {
        task_t *next = task_next(current);
        next->quantum_remaining = next->quantum;
        if (next != task_get_current())
            task_switch(next);
    } else {
        current->quantum_remaining--;
    }
}
