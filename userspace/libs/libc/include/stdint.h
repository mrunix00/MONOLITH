/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

#if defined(MONOLITH_ARCH_IA32) || defined(__i386__)
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
#else
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
#endif

#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  255

#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 65535

#define INT32_MIN  (-2147483647 - 1)
#define INT32_MAX  2147483647
#define UINT32_MAX 4294967295U

#define INT64_MIN  (-9223372036854775807LL - 1)
#define INT64_MAX  9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL

#ifndef SIZE_MAX
#if defined(MONOLITH_ARCH_IA32) || defined(__i386__)
#define SIZE_MAX UINT32_MAX
#else
#define SIZE_MAX UINT64_MAX
#endif
#endif
