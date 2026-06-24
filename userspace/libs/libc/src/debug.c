/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <debug.h>
#include <resource.h>
#include <stdarg.h>
#include <string.h>

#define DEBUG_LOG_BUFFER_SIZE 512

static const char *DEBUG_DEVICE_PATH = "device:/debug";
static rsrc_handle_t _fd = 0;

static void _debug_logv(const char *format, va_list args)
{
    if (_fd == 0) {
        _fd = rsmgr_open(DEBUG_DEVICE_PATH);
        if (_fd < 0)
            return;
    }

    char buf[DEBUG_LOG_BUFFER_SIZE];
    char *p = buf;
    const char *f = format;

    while (*f && (size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1) {
        if (*f != '%') {
            *p++ = *f++;
        } else {
            f++;
            if (*f == '\0') {
                *p++ = '%';
                break;
            }
            switch (*f) {
            case 'd': {
                int value = va_arg(args, int);
                char temp[16];
                char *tp = temp;
                unsigned long u;
                if (value < 0) {
                    *p++ = '-';
                    u = (unsigned long) (-(value + 1)) + 1;
                } else {
                    u = (unsigned long) value;
                }
                /* Convert to string in temp (reversed) */
                if (u == 0) {
                    *tp++ = '0';
                } else {
                    while (u != 0) {
                        *tp++ = '0' + (int) (u % 10);
                        u /= 10;
                    }
                }
                /* Reverse into buf */
                for (char *r = tp - 1; r >= temp && (size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1;
                     r--)
                    *p++ = *r;
                f++;
                break;
            }
            case 'u': {
                unsigned int u_val = va_arg(args, unsigned int);
                unsigned long u = (unsigned long) u_val;
                char temp[16];
                char *tp = temp;
                if (u == 0) {
                    *tp++ = '0';
                } else {
                    while (u != 0) {
                        *tp++ = '0' + (int) (u % 10);
                        u /= 10;
                    }
                }
                for (char *r = tp - 1; r >= temp && (size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1;
                     r--)
                    *p++ = *r;
                f++;
                break;
            }
            case 'x': {
                unsigned int u_val = va_arg(args, unsigned int);
                unsigned long u = (unsigned long) u_val;
                const char *digits = "0123456789abcdef";
                char temp[16];
                char *tp = temp;
                if (u == 0) {
                    *tp++ = '0';
                } else {
                    while (u != 0) {
                        *tp++ = digits[u % 16];
                        u /= 16;
                    }
                }
                for (char *r = tp - 1; r >= temp && (size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1;
                     r--)
                    *p++ = *r;
                f++;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s)
                    s = "(null)";
                while (*s && (size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1)
                    *p++ = *s++;
                f++;
                break;
            }
            case 'c': {
                int c = va_arg(args, int);
                if ((size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1)
                    *p++ = (char) c;
                f++;
                break;
            }
            case '%': {
                if ((size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 1)
                    *p++ = '%';
                f++;
                break;
            }
            default:
                if ((size_t) (p - buf) < DEBUG_LOG_BUFFER_SIZE - 2) {
                    *p++ = '%';
                    *p++ = *f;
                }
                f++;
                break;
            }
        }
    }

    *p = '\0';
    size_t len = (size_t) (p - buf);
    if (len > 0)
        rsmgr_write(_fd, buf, (uint32_t) len);
}

void _debug_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    _debug_logv(format, args);
    va_end(args);
}
