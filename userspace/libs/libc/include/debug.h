/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/*
 * Format and write a debug message to the kernel debug device (device:/debug)
 */
#define debug_log(format, ...) \
    _debug_log(__FILE__ ":" TOSTRING(__LINE__) ": " format, ##__VA_ARGS__)

void _debug_log(const char *format, ...);
