/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/rsmgr/rsmgr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <kernel/arch/pc/x64/task_regs.h>
#elif defined(__i386__)
#include <kernel/arch/pc/ia32/task_regs.h>
#endif

typedef enum task_mode {
    TASK_MODE_KERNEL = 0,
    TASK_MODE_USER = 1,
} task_mode_t;

typedef enum {
    TASK_STATE_RUNNABLE = 0,
    TASK_STATE_SLEEPING,
    TASK_STATE_EXITING,
} task_lifecycle_state_t;

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
    task_t *parent;
    task_t *first_child;
    task_t *next_sibling;
    task_t *prev_sibling;
    uint64_t id;
    char path[256];
    char name[256];
    task_regs_t regs;
    task_mem_t memory;
    rsrc_handle_table_t handle_table;
    uintptr_t stack_bottom;
    unsigned int quantum;
    unsigned int quantum_remaining;
    bool user_mode;
    task_lifecycle_state_t state;
    rsrc_node_t *resource_node;
};

void task_switching_init();
task_t *task_create(void *entry_point, const char *path, task_mode_t mode);
task_t *task_get_current();
void task_set_parent(task_t *task, task_t *parent);
uintptr_t task_find_free_vaddr(task_t *task, size_t num_pages);
task_t *task_find_by_id(uint64_t id);
int task_map(
    task_t *task,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    size_t page_count,
    uintptr_t flags,
    bool release_on_exit);
int task_unmap(task_t *task, uintptr_t virt_addr, size_t page_count, bool release_on_exit);
void task_remove(task_t *task);
void task_switch(task_t *task);
void task_set_state(task_t *task, task_lifecycle_state_t state);
void task_mark_exiting(task_t *task);
task_t *task_next(task_t *task);
task_t *task_idle();
