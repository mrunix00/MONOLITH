/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/idt.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/video/panic.h>
#include <libs/flanterm/src/flanterm.h>

extern struct flanterm_context *_fb_ctx;
static size_t _rows, _cols;
extern bool _term_initialized;

static const char _start_escape[] = "\033[38;5;250;48;5;21m\033[2J\033[H\033[?25l";
static const char _text_color[] = "\x1b[38;5;250;48;5;21m";

void _center_text(const char *text, size_t width)
{
    size_t text_len = vstrlen(text);
    size_t padding = (width > text_len) ? (width - text_len) / 2 : 0;
    for (size_t i = 0; i < padding; i++)
        flanterm_write(_fb_ctx, " ", 1);
    flanterm_write(_fb_ctx, text, strlen(text));

    flanterm_write(_fb_ctx, _text_color, sizeof(_text_color));
    for (size_t i = 0; i < width - text_len - padding; i++)
        flanterm_write(_fb_ctx, " ", 1);
}

void _empty_line()
{
    flanterm_write(_fb_ctx, _text_color, sizeof(_text_color));
    for (size_t i = 0; i < _cols; i++)
        flanterm_write(_fb_ctx, " ", 1);
}

void _print(const char *text)
{
    flanterm_write(_fb_ctx, text, strlen(text));
}

void _print_hex(char *buffer, uint64_t value)
{
    itohex(value, buffer);
    _print(buffer);
}

void _dump_registers(struct interrupt_registers *regs)
{
    char buffer[32];
    _print("\n    Registers state:");
    _print("\n\tRIP = 0x");
    _print_hex(buffer, regs->rip);
    _print("\n\tRAX = 0x");
    _print_hex(buffer, regs->rax);
    _print("\n\tRBX = 0x");
    _print_hex(buffer, regs->rbx);
    _print("\n\tRCX = 0x");
    _print_hex(buffer, regs->rcx);
    _print("\n\tRDX = 0x");
    _print_hex(buffer, regs->rdx);
    _print("\n\tRSP = 0x");
    _print_hex(buffer, regs->rsp);
    _print("\n\tRBP = 0x");
    _print_hex(buffer, regs->rbp);
    _print("\n\tRSI = 0x");
    _print_hex(buffer, regs->rsi);
    _print("\n\tRDI = 0x");
    _print_hex(buffer, regs->rdi);
    _print("\n\tRFLAGS = 0x");
    _print_hex(buffer, regs->rflags);
}

void panic(const char *message, struct interrupt_registers *regs)
{
    if (!_term_initialized)
        return;

    flanterm_get_dimensions(_fb_ctx, &_cols, &_rows);
    flanterm_write(_fb_ctx, _start_escape, sizeof(_start_escape));

    _empty_line();
    _center_text("\x1b[38;5;21;48;5;250m MONOLITH Panic! ", _cols);
    _print("\x1b[38;5;250;48;5;21m\n\n");
    _print("    ");
    _print(message);
    _dump_registers(regs);
}
