/* Minimal hosted-GCC system header for MONOLITH's toolchain sysroot. */
#pragma once

#include <stddef.h>
#include <stdarg.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Opaque stream type/declarations are enough for libgcc's runtime objects. */
typedef struct __monolith_FILE FILE;
extern FILE *stderr;

int sprintf(char *str, const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int fflush(FILE *stream);
long ftell(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
FILE *fdopen(int fd, const char *mode);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int feof(FILE *stream);
