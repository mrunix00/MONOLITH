/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum task_mode {
    TASK_MODE_KERNEL = 0,
    TASK_MODE_USER = 1,
} task_mode_t;

typedef struct
{
    uintptr_t cr3;
    uintptr_t rip;
    uintptr_t rsp;
    uintptr_t rsp0;
    uintptr_t rflags;
    uintptr_t rax;
    uintptr_t rbx;
    uintptr_t rcx;
    uintptr_t rdx;
    uintptr_t rsi;
    uintptr_t rdi;
    uintptr_t rbp;
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uint16_t cs;
    uint16_t ss;
} task_state_t;

typedef struct
{
    uintptr_t phys_addr;
    uintptr_t virt_addr;
    uintptr_t flags;
    size_t page_count;
    bool release_on_exit;
} task_memblock_t;

typedef struct
{
    task_memblock_t *memblocks;
    size_t memblocks_count;
    size_t memblocks_size;
} task_mem_t;

typedef struct task task_t;
struct task
{
    task_t *next;
    task_state_t state;
    task_mem_t memory;
    bool user_mode;
    bool exiting;
};

void task_switching_init();
task_t *task_create(void *entry_point, task_mode_t mode);
task_t *task_get_current();
int task_map(
    task_t *task,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    size_t page_count,
    uintptr_t flags,
    bool release_on_exit);
void task_remove(task_t *task);
void task_switch(task_t *task);
void task_mark_exiting(task_t *task);
task_t *task_next(task_t *task);
task_t *task_idle();
