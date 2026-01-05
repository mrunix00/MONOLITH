/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <assert.h>
#include <stdlib.h>

void __assert_fail(const char *expr, const char *file, int line)
{
    (void) expr;
    (void) file;
    (void) line;
    abort();
}
