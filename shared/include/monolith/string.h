/* Minimal hosted-GCC system header for MONOLITH's toolchain sysroot. */
#pragma once

#include <stddef.h>

double strtod(char *nptr, char **endptr);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
void *memset(void *dest, int c, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *s1, const char *s2);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
