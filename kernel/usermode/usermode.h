/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

void jump_usermode(int (*entry)(int, char **), void *user_stack);
void jump_kernelmode(uintptr_t rip, uintptr_t rsp);
