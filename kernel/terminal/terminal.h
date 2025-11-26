/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libs/limine-protocol/include/limine.h>
#include <stddef.h>

#define TERM_BUFFER_SIZE 1024

/*
 * Reads a single character from the terminal's input.
 * This function flushes the buffer then waits for user input.
 */
char kgetc();

/*
 * Outputs a single character to the terminal.
 * The character will be saved to the buffer and the buffer will only be flushed
 * when it's full, term_flush() is called or '\n' character is encountered.
 */
void kputc(char);

/*
 * Outputs a null-terminated string to the terminal.
 * The string will be saved to the buffer and the buffer will only be flushed
 * when it's full, term_flush() is called or '\n' character is encountered.
 */
void kputs(const char *);

/*
 * Outputs a formatted string to the terminal, similar to printf().
 * The output will be saved to the buffer and the buffer will only be flushed
 * when it's full, term_flush() is called or '\n' character is encountered.
 */
void kprintf(const char *, ...);

/*
 * Forces any buffered output to be written to the terminal.
 * This function calls the terminal's flush_callback function.
 */
void kflush();

/*
 * Initializes the terminal.
 */
void term_init(struct limine_framebuffer_response *response);
