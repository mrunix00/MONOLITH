/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

void qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *));
[[noreturn]] void exit(int status);
[[noreturn]] void abort(void);
int abs(int value);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
