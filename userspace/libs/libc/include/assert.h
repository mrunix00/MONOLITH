/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

void __assert_fail(const char *expr, const char *file, int line);
#define assert(expr) ((expr) ? (void) 0 : __assert_fail(#expr, __FILE__, __LINE__))
