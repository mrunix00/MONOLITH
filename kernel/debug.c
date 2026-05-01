/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/debug.h>
#include <kernel/klibc/string.h>
#include <kernel/timer.h>
#include <libs/flanterm/src/flanterm_backends/fb.h>
#include <stdarg.h>

/*
 * Debug serial port.
 * Set to 0 by default when the serial port is not initialized.
 */
static serial_port_t _debug_port = 0;

/*
 * Flanterm context.
 * Set to NULL by default when the framebuffer is not initialized.
 */
extern struct flanterm_context *_fb_ctx;

static inline void _debug_write_char(char c)
{
    if (_debug_port)
        write_serial(_debug_port, c);
    if (_fb_ctx != NULL)
        flanterm_write(_fb_ctx, &c, 1);
}

static inline void _debug_write_string(const char *str)
{
    if (_debug_port)
        write_string(_debug_port, str);
    if (_fb_ctx != NULL)
        flanterm_write(_fb_ctx, str, strlen(str));
}

static inline void _debug_logu64(uint64_t value)
{
    char buffer[21];
    int i = 0;

    if (value == 0) {
        buffer[i++] = '0';
    } else {
        while (value > 0) {
            buffer[i++] = '0' + (value % 10);
            value /= 10;
        }
    }

    for (int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    buffer[i] = '\0';
    _debug_write_string(buffer);
}

static inline void _debug_log_ms3(uint64_t milliseconds)
{
    _debug_write_char('0' + ((milliseconds / 100) % 10));
    _debug_write_char('0' + ((milliseconds / 10) % 10));
    _debug_write_char('0' + (milliseconds % 10));
}

static inline void _debug_log_timestamp(void)
{
    uint64_t ticks = timer_get_ticks();

    _debug_write_char('[');
    _debug_logu64(ticks / 1000);
    _debug_write_char('.');
    _debug_log_ms3(ticks % 1000);
    _debug_write_string("s] ");
}

bool start_debug_serial(serial_port_t port)
{
    if (init_serial(port)) {
        _debug_port = port;
        debug_log("Started serial debugging\n");
        return true;
    }
    return false;
}

bool start_debug_console(struct limine_framebuffer_response *fb_response)
{
    struct limine_framebuffer *fb = fb_response->framebuffers[0];
    _fb_ctx = flanterm_fb_init(
        NULL,
        NULL,
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
    return _fb_ctx != NULL;
}

void stop_debug_console(void)
{
    if (_fb_ctx != NULL) {
        flanterm_deinit(_fb_ctx, NULL);
        _fb_ctx = NULL;
    }
}

void debug_log(const char *message)
{
    _debug_log_timestamp();
    _debug_write_string(message);
}

static inline void _debug_logd(int d)
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

    _debug_write_string(buffer);
}

static inline void _debug_logx(uint64_t x)
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

    _debug_write_string(buffer);
}

void debug_log_fmt(const char *format, ...)
{
    va_list args;

    if (!_debug_port)
        return;

    va_start(args, format);
    _debug_log_timestamp();
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            /* TODO: Add more format specifiers */
            switch (*format) {
            case 's': {
                const char *str = va_arg(args, const char *);
                _debug_write_string(str);
            } break;
            case 'c': {
                char c = va_arg(args, int);
                _debug_write_char(c);
            } break;
            case 'd':
                _debug_logd(va_arg(args, int));
                break;
            case 'x':
                _debug_logx(va_arg(args, uint64_t));
                break;
            case '%': {
                _debug_write_char('%');
            } break;
            }
        } else {
            _debug_write_char(*format);
        }
        format++;
    }

    va_end(args);
}
