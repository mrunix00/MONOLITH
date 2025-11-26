/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

double strtod(char *nptr, char **endptr);
void *memcpy(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
void *memset(void *, int c, size_t n);
int strcmp(const char *s1, const char *s2);
char *strcat(char *s1, const char *s2);
