/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/tasking/task.h>
#include <stdbool.h>

bool task_domain_init(void);
void task_domain_register(task_t *task);
void task_domain_unregister(task_t *task);
void task_domain_reparent(task_t *task);
