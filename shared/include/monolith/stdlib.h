/* Minimal hosted-GCC system header for MONOLITH's toolchain sysroot. */
#pragma once

#include <stddef.h>

void qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *));
void exit(int status);
__attribute__((__noreturn__)) void abort(void);
int abs(int value);
int atoi(const char *nptr);
char *getenv(const char *name);
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
