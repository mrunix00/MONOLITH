/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

char *strdup(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dst, const char *src, size_t ssize);
char *strstr(const char *str, const char *substr);
size_t strlen(const char *s);
size_t vstrlen(const char *s);
unsigned long atoul(const char *str);
size_t atox(const char *hex);
int atoi(const char *nptr);
char *itohex(unsigned long value, char *buffer);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
