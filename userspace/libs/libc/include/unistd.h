/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2
#define O_ACCMODE 0x3
#define O_CREAT 0x40
#define O_TRUNC 0x200

typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} seek_mode_t;

typedef enum {
    TYPE_FILE,
    TYPE_DIRECTORY,
} file_type_t;

typedef struct
{
    uint64_t size;
    file_type_t type;
} file_stats_t;

int open(const char *path, int flags);

int close(int fd);

int read(int fd, void *buffer, uint32_t size);

int write(int fd, const void *buffer, uint32_t size);

int lseek(int fd, uint32_t offset, seek_mode_t mode);

int fstat(int fd, file_stats_t *stats);

int getdents(int fd, void *buffer, uint32_t size);

int unlink(const char *path);

int mkdir(const char *path, int mode);

int rmdir(const char *path);

#ifdef TEST_ENV
typedef long (*fs_syscall0_fn)(long num);
typedef long (*fs_syscall1_fn)(long num, long arg1);
typedef long (*fs_syscall2_fn)(long num, long arg1, long arg2);
typedef long (*fs_syscall3_fn)(long num, long arg1, long arg2, long arg3);

extern fs_syscall0_fn fs_test_syscall0;
extern fs_syscall1_fn fs_test_syscall1;
extern fs_syscall2_fn fs_test_syscall2;
extern fs_syscall3_fn fs_test_syscall3;
#endif

static inline long syscall0(long num)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall1(long num, long arg1)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall2(long num, long arg1, long arg2)
{
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1), "S"(arg2)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall3(long num, long arg1, long arg2, long arg3)
{
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall4(long num, long arg1, long arg2, long arg3, long arg4)
{
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "c"(arg4)
                     : "r11", "memory");
    return ret;
}

/**
 * Sleep for the specified number of milliseconds.
 * @param ms Number of milliseconds to sleep.
 */
static inline void usleep(uint64_t ms)
{
    syscall1(SYSCALL_SLEEP, (long) ms);
}

/**
 * Get the current tick count (milliseconds since boot).
 * @return Current tick count in milliseconds.
 */
static inline uint64_t get_ticks(void)
{
    return (uint64_t) syscall0(SYSCALL_GET_TICKS);
}

/*
 * Flags for alloc_pages.
 */
#define ALLOC_PAGES_FLAG_RW (1 << 0)   /* Read/Write access (default is read-only) */
#define ALLOC_PAGES_FLAG_EXEC (1 << 1) /* Executable (default is non-executable) */

#define PAGE_SIZE 4096

/**
 * Allocate and map pages into the process's address space.
 * @param num_pages Number of pages to allocate.
 * @param flags Allocation flags (ALLOC_PAGES_FLAG_*).
 * @return Pointer to the allocated memory, or NULL on failure.
 */
static inline void *alloc_pages(size_t num_pages, uint64_t flags)
{
    return (void *) syscall2(SYSCALL_ALLOC_PAGES, (long) num_pages, (long) flags);
}
