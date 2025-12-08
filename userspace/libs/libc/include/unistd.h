/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

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
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall3(long num, long arg1, long arg2, long arg3)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3) : "rcx", "r11", "memory");
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
