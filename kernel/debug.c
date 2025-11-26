/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/debug.h>
#include <kernel/klibc/string.h>
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
static struct flanterm_context *_fb_ctx = NULL;

bool start_debug_serial(serial_port_t port)
{
    if (init_serial(port)) {
        _debug_port = port;
        debug_log("[+] Started serial debugging\n");
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
    if (_debug_port)
        write_string(_debug_port, message);
    if (_fb_ctx != NULL)
        flanterm_write(_fb_ctx, message, strlen(message));
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

    if (_debug_port)
        write_string(_debug_port, buffer);
    if (_fb_ctx != NULL)
        flanterm_write(_fb_ctx, buffer, strlen(buffer));
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

    if (_debug_port)
        write_string(_debug_port, buffer);
    if (_fb_ctx != NULL)
        flanterm_write(_fb_ctx, buffer, strlen(buffer));
}

void debug_log_fmt(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    if (!_debug_port)
        return;

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            /* TODO: Add more format specifiers */
            switch (*format) {
            case 's': {
                const char *str = va_arg(args, const char *);
                if (_debug_port)
                    write_string(_debug_port, str);
                if (_fb_ctx != NULL)
                    flanterm_write(_fb_ctx, str, strlen(str));
            } break;
            case 'c': {
                char c = va_arg(args, int);
                if (_debug_port)
                    write_serial(_debug_port, c);
                if (_fb_ctx != NULL)
                    flanterm_write(_fb_ctx, &c, 1);
            } break;
            case 'd':
                _debug_logd(va_arg(args, int));
                break;
            case 'x':
                _debug_logx(va_arg(args, uint64_t));
                break;
            case '%': {
                if (_debug_port)
                    write_serial(_debug_port, '%');
                if (_fb_ctx != NULL)
                    flanterm_write(_fb_ctx, "%", 1);
            } break;
            }
        } else {
            if (_debug_port)
                write_serial(_debug_port, *format);
            if (_fb_ctx != NULL)
                flanterm_write(_fb_ctx, format, 1);
        }
        format++;
    }
}
