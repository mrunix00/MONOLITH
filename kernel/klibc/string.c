/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <stdint.h>

int strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2 && *s1; s1++, s2++)
        ;
    return *(unsigned char *) s1 - *(unsigned char *) s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    const unsigned char *l = (void *) s1, *r = (void *) s2;
    if (!n--)
        return 0;
    for (; *l && *r && n && *l == *r; l++, r++, n--)
        ;
    return *l - *r;
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

char *strncat(char *dst, const char *src, size_t ssize)
{
    char *result = dst;
    while (*dst)
        dst++;
    while (ssize-- && (*dst++ = *src++))
        ;
    *dst = '\0';
    return result;
}

char *strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
    return dst - 1;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *original_dest = dest;
    while (n > 0 && *src != '\0') {
        *dest++ = *src++;
        n--;
    }
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    return original_dest;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

size_t vstrlen(const char *s)
{
    size_t visual_len = 0;
    size_t i = 0;
    while (s[i] != '\0') {
        if (s[i] == '\x1b' && s[i + 1] == '[') {
            i += 2;
            while (s[i] != '\0' && !((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
                i++;
            }
            if (s[i] != '\0')
                i++;
        } else {
            visual_len++;
            i++;
        }
    }
    return visual_len;
}

char *strstr(const char *str, const char *substr)
{
    if (*substr == '\0')
        return (char *) str;
    while (*str != '\0') {
        const char *s1 = str;
        const char *s2 = substr;
        while (*s1 == *s2 && *s2 != '\0') {
            s1++;
            s2++;
        }
        if (*s2 == '\0')
            return (char *) str;
        str++;
    }
    return NULL;
}

unsigned long atoul(const char *str)
{
    unsigned long num = 0;
    while (*str == ' ' || *str == '\t')
        str++;
    while (*str >= '0' && *str <= '9') {
        num = num * 10 + (*str - '0');
        str++;
    }
    return num;
}

int atoi(const char *nptr)
{
    int result = 0;
    int sign = 1;

    while (*nptr == ' ' || *nptr == '\t')
        nptr++;

    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }

    return result * sign;
}

size_t atox(const char *hex)
{
    size_t result = 0;

    // Skip '0x' or '0X' prefix if present
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }

    while (*hex) {
        result *= 16;

        if (*hex >= '0' && *hex <= '9') {
            result += *hex - '0';
        } else if (*hex >= 'a' && *hex <= 'f') {
            result += *hex - 'a' + 10;
        } else if (*hex >= 'A' && *hex <= 'F') {
            result += *hex - 'A' + 10;
        }

        hex++;
    }

    return result;
}

char *itohex(size_t x, char *buffer)
{
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
    return buffer;
}

char *strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *dup = kmalloc(len);
    if (!dup)
        return NULL;
    memcpy(dup, str, len);
    return dup;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == c)
            return (char *) s;
        s++;
    }
    return NULL;
}

char *strrchr(const char *s, int c)
{
    char *last = NULL;
    while (*s) {
        if (*s == c)
            last = (char *) s;
        s++;
    }
    return last;
}
