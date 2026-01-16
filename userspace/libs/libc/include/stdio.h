/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

typedef enum {
    stderr,
} io_t;

int sprintf(char *str, const char *format, ...);
