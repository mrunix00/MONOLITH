/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stdarg.h>
#include <stdio.h>

int fprintf(io_t err, const char *fmt, ...)
{
    // TODO: Implement fprintf function
    return 0;
}

static char *utoa(char *str, unsigned long num, int base, int uppercase)
{
    const char *digits;
    if (uppercase) {
        digits = "0123456789ABCDEF";
    } else {
        digits = "0123456789abcdef";
    }

    char temp[32];
    int i = 0;

    if (num == 0) {
        *str++ = '0';
        return str;
    }

    while (num != 0) {
        unsigned long remainder = num % base;
        num = num / base;
        temp[i++] = digits[remainder];
    }

    for (int j = i - 1; j >= 0; j--) {
        *str++ = temp[j];
    }

    return str;
}

static char *ftoa(char *str, double value, int precision)
{
    if (value < 0) {
        *str++ = '-';
        value = -value;
    }

    if (precision == 0) {
        unsigned long integer_part = (unsigned long) (value + 0.5);
        str = utoa(str, integer_part, 10, 0);
    } else {
        double factor = 1.0;
        for (int i = 0; i < precision; i++) {
            factor *= 10.0;
        }

        unsigned long integer_part = (unsigned long) value;
        double fractional = value - integer_part;
        fractional *= factor;
        unsigned long fractional_int = (unsigned long) (fractional + 0.5);

        if (fractional_int >= (unsigned long) factor) {
            fractional_int = 0;
            integer_part++;
        }

        str = utoa(str, integer_part, 10, 0);
        *str++ = '.';

        unsigned long divisor = 1;
        for (int i = 0; i < precision - 1; i++) {
            divisor *= 10;
        }

        for (int i = 0; i < precision; i++) {
            if (divisor == 0) {
                *str++ = '0';
            } else {
                unsigned long digit = fractional_int / divisor;
                *str++ = '0' + digit;
                fractional_int -= digit * divisor;
                divisor /= 10;
            }
        }
    }
    return str;
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char *p = str;
    const char *f = format;

    while (*f) {
        if (*f != '%') {
            *p++ = *f++;
        } else {
            f++;
            if (*f == '\0') {
                *p++ = '%';
                break;
            }

            if (f[0] == '.' && f[1] == 'f') {
                double value = va_arg(args, double);
                p = ftoa(p, value, 0);
                f += 2;
            } else if (f[0] == '.' && f[1] == '0' && f[2] == 'f') {
                double value = va_arg(args, double);
                p = ftoa(p, value, 0);
                f += 3;
            } else if (f[0] == '.' && f[1] >= '0' && f[1] <= '9' && f[2] == 'f') {
                int precision = f[1] - '0';
                double value = va_arg(args, double);
                p = ftoa(p, value, precision);
                f += 3;
            } else if (
                f[0] == '.' && f[1] >= '0' && f[1] <= '9' && f[2] >= '0' && f[2] <= '9'
                && f[3] == 'f') {
                int precision = (f[1] - '0') * 10 + (f[2] - '0');
                if (precision > 6)
                    precision = 6;
                double value = va_arg(args, double);
                p = ftoa(p, value, precision);
                f += 4;
            } else if (f[0] == '0' && f[1] == '2' && f[2] == 'X') {
                unsigned int value = va_arg(args, unsigned int);
                char temp[32];
                char *end = utoa(temp, (unsigned long) value, 16, 1);
                *end = '\0';
                int len = 0;
                for (char *t = temp; *t; t++, len++)
                    ;
                if (len < 2) {
                    for (int i = 0; i < 2 - len; i++) {
                        *p++ = '0';
                    }
                }
                for (int i = 0; i < len; i++) {
                    *p++ = temp[i];
                }
                f += 3;
            } else if (f[0] == '0' && f[1] == '2' && f[2] == 'x') {
                unsigned int value = va_arg(args, unsigned int);
                char temp[32];
                char *end = utoa(temp, (unsigned long) value, 16, 0);
                *end = '\0';
                int len = 0;
                for (char *t = temp; *t; t++, len++)
                    ;
                if (len < 2) {
                    for (int i = 0; i < 2 - len; i++) {
                        *p++ = '0';
                    }
                }
                for (int i = 0; i < len; i++) {
                    *p++ = temp[i];
                }
                f += 3;
            } else {
                char spec = *f;
                f++;
                switch (spec) {
                case 'd': {
                    int value = va_arg(args, int);
                    unsigned long u;
                    if (value < 0) {
                        *p++ = '-';
                        u = (unsigned long) (-(value + 1)) + 1;
                    } else {
                        u = (unsigned long) value;
                    }
                    p = utoa(p, u, 10, 0);
                    break;
                }
                case 'u': {
                    unsigned int u_val = va_arg(args, unsigned int);
                    p = utoa(p, (unsigned long) u_val, 10, 0);
                    break;
                }
                case 'x': {
                    unsigned int u_val = va_arg(args, unsigned int);
                    p = utoa(p, (unsigned long) u_val, 16, 0);
                    break;
                }
                case 'X': {
                    unsigned int u_val = va_arg(args, unsigned int);
                    p = utoa(p, (unsigned long) u_val, 16, 1);
                    break;
                }
                case 'c': {
                    int c = va_arg(args, int);
                    *p++ = (char) c;
                    break;
                }
                case 's': {
                    char *s = va_arg(args, char *);
                    while (*s) {
                        *p++ = *s++;
                    }
                    break;
                }
                case 'f': {
                    double value = va_arg(args, double);
                    p = ftoa(p, value, 6);
                    break;
                }
                case '%': {
                    *p++ = '%';
                    break;
                }
                default: {
                    *p++ = '%';
                    *p++ = spec;
                    break;
                }
                }
            }
        }
    }

    *p = '\0';
    va_end(args);
    return (int) (p - str);
}
