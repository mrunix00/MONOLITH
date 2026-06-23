/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef enum {
    stderr,
} io_t;

int sprintf(char *str, const char *format, ...);
