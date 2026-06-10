/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#define CHAR_BIT 8

#define SCHAR_MAX 127
#define SCHAR_MIN (-128)
#define UCHAR_MAX 255

#define SHRT_MAX 32767
#define SHRT_MIN (-32768)
#define USHRT_MAX 65535

#define INT_MAX 2147483647
#define INT_MIN (-2147483647 - 1)
#define UINT_MAX 0xffffffffU

#if defined(MONOLITH_ARCH_IA32) || defined(__i386__)
#define LONG_MAX 2147483647L
#define LONG_MIN (-2147483647L - 1)
#define ULONG_MAX 0xffffffffUL
#else
#define LONG_MAX 9223372036854775807L
#define LONG_MIN (-9223372036854775807L - 1)
#define ULONG_MAX 0xffffffffffffffffUL
#endif

#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-9223372036854775807LL - 1)
#define ULLONG_MAX 0xffffffffffffffffULL

#ifndef SIZE_MAX
#define SIZE_MAX ULONG_MAX
#endif
