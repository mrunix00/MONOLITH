/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/serial.h>
#include <libs/limine-protocol/include/limine.h>
#include <stdbool.h>

/*
 * Start the serial port for debugging.
 * Returns true if serial port debugging was started successfully, false otherwise.
 */
bool start_debug_serial(serial_port_t port);

/*
 * Start the console for debugging.
 * Returns true if console debugging was started successfully, false otherwise.
 */
bool start_debug_console(struct limine_framebuffer_response *fb_response);

/*
 * Stop the console for debugging.
 */
void stop_debug_console(void);

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/*
 * Print a debug message.
 */
#define debug_log(message) _debug_log(__FILE__ ":" TOSTRING(__LINE__) ": " message)

/*
 * Print a formatted debug message.
 */
#define debug_log_fmt(format, ...) \
    _debug_log_fmt(__FILE__ ":" TOSTRING(__LINE__) ": " format, ##__VA_ARGS__)

#define debug_assert(expr) _debug_assert(expr, __FILE__ ":" TOSTRING(__LINE__), #expr)

void _debug_log(const char *message);

void _debug_log_fmt(const char *format, ...);

bool _debug_assert(bool expr, const char *line, const char *expr_str);
