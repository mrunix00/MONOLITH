/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/tasking/task.h>

/*
 * Load and execute an executable file into memory.
 * Returns 0 on success, -1 on failure.
 */
task_t *load_elf(const char *path);
task_t *load_elf_for_parent(const char *path, task_t *parent);
