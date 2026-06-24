/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/debug.h>
#include <kernel/devices/device_domain.h>
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

static volatile int _debug_lock = 0;

static inline void _debug_lock_acquire()
{
    while (__sync_lock_test_and_set(&_debug_lock, 1))
        __asm__ volatile("pause");
}

static inline void _debug_lock_release()
{
    __sync_lock_release(&_debug_lock);
}

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

bool start_debug_console(framebuffer_t *fb)
{
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

void _debug_log(const char *message)
{
    _debug_lock_acquire();
    _debug_log_timestamp();
    _debug_write_string(message);
    _debug_lock_release();
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

static inline void _debug_logx(uintptr_t x)
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

void _debug_log_fmt(const char *format, ...)
{
    va_list args;

    if (!_debug_port)
        return;

    _debug_lock_acquire();
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
                _debug_logx(va_arg(args, uintptr_t));
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
    _debug_lock_release();
}

bool _debug_assert(bool expr, const char *line, const char *expr_str)
{
    if (!expr)
        _debug_log_fmt("%s: Assertion \"%s\" failed\n", line, expr_str);
    return expr;
}

static rsrc_status_t _debug_dev_write(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    (void) resource;
    (void) handle_state;

    if (buffer == NULL || buffer_len == 0) {
        if (out_bytes_written != NULL)
            *out_bytes_written = 0;
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _debug_log(buffer);

    if (out_bytes_written != NULL)
        *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _debug_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = NULL,
    .list = NULL,
    .read = NULL,
    .write = _debug_dev_write,
    .mmap = NULL,
    .poll = NULL,
    .remove = NULL,
    .control = NULL,
};

void debug_device_init()
{
    device_register("/debug", &_debug_ops, NULL);
}
