/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/klibc/memory.h>

void *memset(void *dest, int value, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        ((unsigned char *) dest)[i] = (unsigned char) value;
    }
    return dest;
}

void memcpy(void *dest, const void *src, size_t n)
{
    char *d = (char *) dest;
    const char *s = (const char *) src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
}
