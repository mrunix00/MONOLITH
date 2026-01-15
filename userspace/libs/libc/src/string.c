/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <string.h>

double strtod(char *nptr, char **endptr)
{
    const char *p = nptr;
    double value = 0.0;
    int sign = 1;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+')
        p++;

    while (*p >= '0' && *p <= '9') {
        value = value * 10.0 + (*p - '0');
        p++;
    }

    if (*p == '.') {
        double fraction = 0.1;
        p++;
        while (*p >= '0' && *p <= '9') {
            value += (*p - '0') * fraction;
            fraction *= 0.1;
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        int exp_sign = 1;
        int exponent = 0;

        if (*p == '-') {
            exp_sign = -1;
            p++;
        } else if (*p == '+')
            p++;

        while (*p >= '0' && *p <= '9') {
            exponent = exponent * 10 + (*p - '0');
            p++;
        }

        for (int i = 0; i < exponent; i++) {
            if (exp_sign > 0) {
                value *= 10.0;
            } else {
                value /= 10.0;
            }
        }
    }

    if (endptr != NULL)
        *endptr = (char *) p;
    return sign * value;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char) c;
    return s;
}

char *strcat(char *dest, const char *src)
{
    char *result = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
    return result;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return ((unsigned char) *s1) - ((unsigned char) *s2);
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return ((unsigned char) *s1) - ((unsigned char) *s2);
}

char *strcpy(char *dest, const char *src)
{
    char *result = dest;
    while ((*dest++ = *src++))
        ;
    return result;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *result = dest;
    while (n > 0 && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    return result;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dest;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && s[len] != '\0')
        len++;
    return len;
}
