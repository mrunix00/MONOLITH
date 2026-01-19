/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

/* When testing on host system, use GCC's built-in va_arg implementation */
#ifdef TEST_ENV
#include_next <stdarg.h>
#else

typedef char* va_list;

#define va_start(ap, last) ((ap) = (va_list)&(last) + sizeof(last))
#define va_arg(ap, type) (*((type*)((ap) += sizeof(type))) - sizeof(type))
#define va_end(ap) ((void)0)

#endif
