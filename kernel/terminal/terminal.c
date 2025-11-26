/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "kernel/debug.h"
#include <kernel/input/ps2_keyboard.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/memstat.h>
#include <kernel/terminal/kshell.h>
#include <kernel/terminal/terminal.h>
#include <libs/flanterm/src/flanterm_backends/fb.h>
#include <stdarg.h>
#include <stdint.h>

static int _index = 0;
static char _buffer[TERM_BUFFER_SIZE];
bool _term_initialized = false;

extern struct flanterm_context *_fb_ctx;

static bool _is_capslock_on = false;
static bool _is_lshift_pressed = false;
static bool _is_rshift_pressed = false;
static bool _waiting_for_key = false;
static keyboard_event_t _last_event;

void kputc(char c)
{
    if (c == '\n') {
        if (_index >= TERM_BUFFER_SIZE) {
            kflush();
        }
        _buffer[_index++] = '\n';
        kflush();
        return;
    } else if (_index >= TERM_BUFFER_SIZE) {
        kflush();
    }
    _buffer[_index++] = c;
}

void kputs(const char *str)
{
    while (*str)
        kputc(*str++);
}

static inline void _term_printd(int d)
{
    char buffer[16];
    int i = 0, is_negative = 0;

    if (d < 0) {
        is_negative = 1;
        d = -d;
    }

    if (d == 0) {
        buffer[i++] = '0';
    } else {
        while (d > 0) {
            buffer[i++] = '0' + (d % 10);
            d /= 10;
        }

        if (is_negative)
            buffer[i++] = '-';
    }

    /* Reverse the string */
    for (int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    buffer[i] = '\0';

    kputs(buffer);
}

static inline void _term_printx(size_t x)
{
    char buffer[16];
    int i = 0;

    if (x == 0) {
        buffer[i++] = '0';
    } else {
        while (x > 0) {
            uint8_t digit = x % 16;
            if (digit < 10)
                buffer[i++] = '0' + digit;
            else
                buffer[i++] = 'a' + (digit - 10);
            x /= 16;
        }
    }

    /* Reverse the string */
    for (int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    buffer[i] = '\0';

    kputs(buffer);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt != '\0') {
        if (*fmt == '%') {
            fmt++;
            /* TODO: Add more format specifiers */
            switch (*fmt) {
            case 's':
                kputs(va_arg(args, const char *));
                break;
            case 'c':
                kputc(va_arg(args, int));
                break;
            case 'd':
                _term_printd(va_arg(args, int));
                break;
            case 'x':
                _term_printx(va_arg(args, uint64_t));
                break;
            case '%':
                kputc('%');
                break;
            }
        } else {
            kputc(*fmt);
        }
        fmt++;
    }

    va_end(args);
}

static void _keyboard_event_handler(keyboard_event_t event)
{
    switch (event.scancode) {
    case KEY_LSHIFT:
        _is_lshift_pressed = (event.action == KEYBOARD_PRESSED);
        break;
    case KEY_RSHIFT:
        _is_rshift_pressed = (event.action == KEYBOARD_PRESSED);
        break;
    case KEY_CAPSLOCK:
        if (event.action == KEYBOARD_PRESSED) {
            _is_capslock_on = !_is_capslock_on;
        }
        break;
    default:
        if (event.action == KEYBOARD_PRESSED || event.action == KEYBOARD_HOLD) {
            _last_event = event;
            _waiting_for_key = false;
        }
        break;
    }
}

char kgetc()
{
    kflush();
    _waiting_for_key = true;
    while (_waiting_for_key)
        __asm__("hlt");

    const keyboard_layout_t *layout = &keyboard_layouts[KB_LAYOUT_US];
    uint8_t scancode = (uint8_t) _last_event.scancode;

    bool shift_active = _is_lshift_pressed || _is_rshift_pressed;
    const char *keymap = shift_active ? layout->shifted_keymap : layout->keymap;

    char c = 0;
    if (scancode < sizeof(layout->keymap))
        c = keymap[scancode];

    /* Handle CapsLock transformation */
    if (c >= 'a' && c <= 'z') {
        if (_is_capslock_on)
            c -= 32;
    } else if (c >= 'A' && c <= 'Z') {
        if (_is_capslock_on)
            c += 32;
    }

    return c;
}

void kflush()
{
    if (_index > 0) {
        flanterm_write(_fb_ctx, _buffer, _index);
        _index = 0;
    }
}

void term_init(struct limine_framebuffer_response *response)
{
    if (_fb_ctx != NULL)
        flanterm_deinit(_fb_ctx, NULL);

    struct limine_framebuffer *fb = response->framebuffers[0];
    _fb_ctx = flanterm_fb_init(
        (void *) kmalloc,
        (void *) kfree,
        fb->address,
        fb->width,
        fb->height,
        fb->pitch,
        fb->red_mask_size,
        fb->red_mask_shift,
        fb->green_mask_size,
        fb->green_mask_shift,
        fb->blue_mask_size,
        fb->blue_mask_shift,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        1,
        0,
        0,
        0);
    _index = 0;

    ps2_keyboard_register_event_handler(_keyboard_event_handler);
    _term_initialized = true;

    kshell_init();
    memstat_init_cmds();
    kshell_launch();
}
