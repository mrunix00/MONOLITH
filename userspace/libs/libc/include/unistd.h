/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>

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
