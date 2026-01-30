/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#ifdef NDEBUG
#define assert(expr) ((void) 0)
#else
#include <stdlib.h>
#define assert(expr) ((expr) ? (void) 0 : abort())
#endif
