/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

void load_kernel_args(const char *cmdline);
bool get_kernel_arg(const char *key, char *out_value, size_t out_value_len);
