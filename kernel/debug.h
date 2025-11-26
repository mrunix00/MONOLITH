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

/*
 * Print a debug message.
 */
void debug_log(const char *message);

/*
 * Print a formatted debug message.
 */
void debug_log_fmt(const char *format, ...);
