/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#define DEFAULT_QUANTUM 10

#include <kernel/tasking/task.h>

void scheduler_init();
void scheduler_tick();
