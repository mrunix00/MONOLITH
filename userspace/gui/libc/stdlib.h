/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

void qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *));
void abort();
