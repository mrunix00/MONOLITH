/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/sse.h>
#include <kernel/devices/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/rsmgr/rsmgr.h>
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task_domain.h>

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR 0x1B
#define USER_DATA_SELECTOR 0x23
#define DEFAULT_RFLAGS 0x202
#define KERNEL_STACK_SIZE 0x4000
#define USER_SPACE_START 0x0000000000001000ULL
#define USER_SPACE_END 0x0000800000000000ULL

static task_t _task_list_head;
static task_t *_task_list_tail;
static task_t *_current_task;
static task_t *_next_task;
static task_t *_deferred_destroy_list;
static uint64_t _next_task_id = 1;

extern void _task_switch_gate_stub();

static void _task_remove_children(task_t *task)
{
    if (!debug_assert(task))
        return;

    while (task->first_child)
        task_remove(task->first_child);
}

static void _task_unlink(task_t *task)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(task != &_task_list_head))
        return;

    task_t *prev = &_task_list_head;
    while (prev->next && prev->next != &_task_list_head) {
        if (prev->next == task)
            break;
        prev = prev->next;
    }

    if (prev->next != task)
        return;

    prev->next = task->next ? task->next : &_task_list_head;
    if (_task_list_tail == task)
        _task_list_tail = prev;
}

static void _task_unlink_child(task_t *task)
{
    if (!debug_assert(task))
        return;

    if (!task->parent)
        return debug_log_fmt("Failed to unlink child: %d has no parent\n", task->id);

    if (task->prev_sibling)
        task->prev_sibling->next_sibling = task->next_sibling;
    else
        task->parent->first_child = task->next_sibling;

    if (task->next_sibling)
        task->next_sibling->prev_sibling = task->prev_sibling;

    task->parent = NULL;
    task->next_sibling = NULL;
    task->prev_sibling = NULL;
}

static void _task_destroy(task_t *task)
{
    if (!debug_assert(task))
        return;

    _task_remove_children(task);
    _task_unlink_child(task);

    syscalls_task_cleanup(task);
    ipc_task_cleanup(task);
    task_domain_unregister(task);

    if (task->memory.memblocks) {
        for (size_t i = 0; i < task->memory.memblocks_count; i++) {
            task_memblock_t *memblock = &task->memory.memblocks[i];
            if (!memblock->release_on_exit)
                continue;
            if (memblock->phys_addr)
                pmm_free((void *) memblock->phys_addr, memblock->page_count);
        }
        kfree(task->memory.memblocks);
        task->memory.memblocks = NULL;
    }

    if (task->user_mode && task->regs.cr3 != 0) {
        vmm_destroy_address_space(task->regs.cr3);
        task->regs.cr3 = 0;
    }

    kfree(task->regs.fx_state);
    kfree((void *) task->stack_bottom);
    kfree(task);
}

static const char *_task_basename(const char *path)
{
    if (path == NULL)
        return "";

    const char *name = strrchr(path, '/');
    return name == NULL ? path : name + 1;
}

static void _task_defer_destroy(task_t *task)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(task != &_task_list_head))
        return;

    task->next = _deferred_destroy_list;
    _deferred_destroy_list = task;
}

static void _task_destroy_deferred(void)
{
    while (_deferred_destroy_list) {
        task_t *task = _deferred_destroy_list;
        _deferred_destroy_list = task->next;
        task->next = NULL;
        _task_destroy(task);
    }
}

static void _task_state_save(task_t *task, interrupt_registers_t *regs)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(regs))
        return;

    task->regs.rax = regs->rax;
    task->regs.rbx = regs->rbx;
    task->regs.rcx = regs->rcx;
    task->regs.rdx = regs->rdx;
    task->regs.rsi = regs->rsi;
    task->regs.rdi = regs->rdi;
    task->regs.rbp = regs->rbp;
    task->regs.r8 = regs->r8;
    task->regs.r9 = regs->r9;
    task->regs.r10 = regs->r10;
    task->regs.r11 = regs->r11;
    task->regs.r12 = regs->r12;
    task->regs.r13 = regs->r13;
    task->regs.r14 = regs->r14;
    task->regs.r15 = regs->r15;
    task->regs.rip = regs->rip;
    task->regs.rsp = regs->rsp;
    task->regs.rflags = regs->rflags ? regs->rflags : DEFAULT_RFLAGS;
    task->regs.cr3 = asm_read_cr3();

    uint16_t cs = (uint16_t) regs->cs;
    uint16_t ss = (uint16_t) regs->ss;
    task->regs.cs = cs ? cs : (task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR);
    task->regs.ss = ss ? ss : (task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR);

    if (task->regs.fx_state_aligned)
        sse_save(task->regs.fx_state_aligned);
}

static void _task_state_load(task_t *task, interrupt_registers_t *regs)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(regs))
        return;

    regs->rax = task->regs.rax;
    regs->rbx = task->regs.rbx;
    regs->rcx = task->regs.rcx;
    regs->rdx = task->regs.rdx;
    regs->rsi = task->regs.rsi;
    regs->rdi = task->regs.rdi;
    regs->rbp = task->regs.rbp;
    regs->r8 = task->regs.r8;
    regs->r9 = task->regs.r9;
    regs->r10 = task->regs.r10;
    regs->r11 = task->regs.r11;
    regs->r12 = task->regs.r12;
    regs->r13 = task->regs.r13;
    regs->r14 = task->regs.r14;
    regs->r15 = task->regs.r15;
    regs->rip = task->regs.rip;
    regs->rsp = task->regs.rsp;
    regs->rflags = task->regs.rflags ? task->regs.rflags : DEFAULT_RFLAGS;

    uint16_t cs = task->regs.cs ? task->regs.cs
                                 : (task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR);
    uint16_t ss = task->regs.ss ? task->regs.ss
                                 : (task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR);

    /* Ensure SS.RPL matches the CPL from CS */
    if ((ss & 0x03) != (cs & 0x03))
        ss = (cs & 0x03) == 0 ? KERNEL_DATA_SELECTOR : USER_DATA_SELECTOR;

    regs->cs = cs;
    regs->ss = ss;

    if (task->regs.fx_state_aligned)
        sse_restore(task->regs.fx_state_aligned);
}

task_t *task_create(void *entry_point, const char *path, task_mode_t mode)
{
    task_t *task = (task_t *) kmalloc(sizeof(task_t));
    if (!task) {
        debug_log("Failed to create task: kmalloc failed\n");
        return NULL;
    }

    memset(task, 0, sizeof(task_t));
    task->id = _next_task_id++;
    if (_next_task_id == 0)
        _next_task_id = 1;
    task->user_mode = (mode == TASK_MODE_USER);
    task->regs.rip = (uintptr_t) entry_point;
    task->regs.rflags = DEFAULT_RFLAGS;
    task->regs.cs = task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR;
    task->regs.ss = task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR;
    task->quantum = DEFAULT_QUANTUM;

    task->regs.fx_state = kmalloc(512 + 16);
    if (!task->regs.fx_state) {
        debug_log("Failed to create task: kmalloc failed\n");
        kfree(task);
        return NULL;
    }
    uintptr_t fx_addr = (uintptr_t) task->regs.fx_state;
    task->regs.fx_state_aligned = (void *) ((fx_addr + 15) & ~((uintptr_t) 0xF));
    memset(task->regs.fx_state_aligned, 0, 512);

    /* Initialize FPU state with defaults */
    uint8_t *fx_region = (uint8_t *) task->regs.fx_state_aligned;
    *((uint16_t *) &fx_region[0]) = 0x037F;  /* FCW */
    *((uint32_t *) &fx_region[24]) = 0x1F80; /* MXCSR */

    task->stack_bottom = (uintptr_t) kmalloc(KERNEL_STACK_SIZE);
    if (!task->stack_bottom) {
        debug_log("Failed to create task: kmalloc failed\n");
        kfree(task->regs.fx_state);
        kfree(task);
        return NULL;
    }
    task->regs.rsp0 = task->stack_bottom + KERNEL_STACK_SIZE;

    if (rsmgr_handle_table_init(&task->handle_table) != RSRC_STATUS_OK) {
        debug_log("Failed to create task: handle table init failed\n");
        kfree((void *) task->stack_bottom);
        kfree(task->regs.fx_state);
        kfree(task);
        return NULL;
    }

    if (task->user_mode) {
        task->regs.cr3 = vmm_create_address_space();
        if (task->regs.cr3 == 0) {
            debug_log("Failed to create task: vmm_create_address_space failed\n");
            rsmgr_handle_table_destroy(&task->handle_table);
            kfree((void *) task->stack_bottom);
            kfree(task->regs.fx_state);
            kfree(task);
            return NULL;
        }
    } else {
        task->regs.cr3 = vmm_get_kernel_cr3();
    }

    const char *task_path = path == NULL ? "" : path;
    strncpy(task->path, task_path, sizeof(task->path) - 1);
    task->path[sizeof(task->path) - 1] = '\0';
    const char *task_name = _task_basename(task_path);
    strncpy(task->name, task_name, sizeof(task->name) - 1);
    task->name[sizeof(task->name) - 1] = '\0';

    task->next = &_task_list_head;
    _task_list_tail->next = task;
    _task_list_tail = task;

    task->resource_node = NULL;
    if (task->user_mode)
        task_domain_register(task);

    return task;
}

void task_set_parent(task_t *child, task_t *parent)
{
    if (!debug_assert(child))
        return;
    if (!debug_assert(child != parent))
        return;
    if (!debug_assert(child != &_task_list_head))
        return;

    if (child->parent)
        _task_unlink_child(child);

    if (!parent || parent->state == TASK_STATE_EXITING)
        return;

    child->parent = parent;
    child->prev_sibling = NULL;
    child->next_sibling = parent->first_child;
    if (parent->first_child)
        parent->first_child->prev_sibling = child;
    parent->first_child = child;
    task_domain_reparent(child);
}

int task_map(
    task_t *task,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    size_t page_count,
    uintptr_t flags,
    bool release_on_exit)
{
    if (!debug_assert(task))
        return -1;
    if (!debug_assert(page_count != 0))
        return -1;

    if (task->memory.memblocks == NULL) {
        task->memory.memblocks_count = 0;
        task->memory.memblocks_size = 16;
        task->memory.memblocks = (task_memblock_t *) kmalloc(
            sizeof(task_memblock_t) * task->memory.memblocks_size);
        if (!task->memory.memblocks) {
            debug_log("Failed to map: kmalloc failed\n");
            return -1;
        }
    }

    if (task->memory.memblocks_count == task->memory.memblocks_size) {
        task_memblock_t *new_memblocks = (task_memblock_t *) krealloc(
            task->memory.memblocks, sizeof(task_memblock_t) * task->memory.memblocks_size * 2);
        if (!new_memblocks) {
            debug_log("Failed to map: krealloc failed\n");
            return -1;
        }
        task->memory.memblocks = new_memblocks;
        task->memory.memblocks_size *= 2;
    }

    task_memblock_t *memblock = &task->memory.memblocks[task->memory.memblocks_count++];
    memblock->virt_addr = virt_addr;
    memblock->phys_addr = phys_addr;
    memblock->page_count = page_count;
    memblock->flags = flags;
    memblock->release_on_exit = release_on_exit;

    /* Map into the task's own address space */
    vmm_map_range(task->regs.cr3, virt_addr, phys_addr, page_count * PAGE_SIZE, flags, true);

    return 0;
}

task_t *task_get_current()
{
    return _current_task;
}

uintptr_t task_find_free_vaddr(task_t *task, size_t num_pages)
{
    if (!debug_assert(task))
        return 0;
    if (!debug_assert(num_pages != 0))
        return 0;

    uintptr_t candidate = USER_SPACE_START;
    size_t required = num_pages * PAGE_SIZE;
    bool adjusted;
    do {
        adjusted = false;
        for (size_t i = 0; i < task->memory.memblocks_count; i++) {
            task_memblock_t *b = &task->memory.memblocks[i];
            uintptr_t block_end = b->virt_addr + b->page_count * PAGE_SIZE;
            if (candidate < block_end && candidate + required > b->virt_addr) {
                candidate = block_end;
                adjusted = true;
                break;
            }
        }
    } while (adjusted);

    if (candidate < USER_SPACE_START || candidate + required > USER_SPACE_END)
        return 0;
    return candidate;
}

task_t *task_find_by_id(uint64_t id)
{
    if (id == 0)
        return NULL;
    for (task_t *t = _task_list_head.next; t && t != &_task_list_head; t = t->next) {
        if (t->id == id)
            return t;
    }
    return NULL;
}

int task_unmap(task_t *task, uintptr_t virt_addr, size_t page_count, bool release_on_exit)
{
    if (!debug_assert(task))
        return -1;
    if (!debug_assert(page_count != 0))
        return -1;

    vmm_unmap_range(task->regs.cr3, virt_addr, page_count * PAGE_SIZE, true);

    if (!task->memory.memblocks || task->memory.memblocks_count == 0)
        return 0;

    for (size_t i = 0; i < task->memory.memblocks_count; i++) {
        task_memblock_t *memblock = &task->memory.memblocks[i];
        if (memblock->virt_addr != virt_addr || memblock->page_count != page_count)
            continue;
        if (release_on_exit && memblock->release_on_exit && memblock->phys_addr)
            pmm_free((void *) memblock->phys_addr, memblock->page_count);

        /* Shift remaining entries down */
        for (size_t j = i + 1; j < task->memory.memblocks_count; j++)
            task->memory.memblocks[j - 1] = task->memory.memblocks[j];
        task->memory.memblocks_count--;
        break;
    }

    return 0;
}

void task_remove(task_t *task)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(task != &_task_list_head))
        return;

    _task_remove_children(task);

    bool removing_current = (_current_task == task);
    if (removing_current)
        _current_task = task->next ? task->next : &_task_list_head;

    _task_unlink(task);
    if (removing_current)
        _task_defer_destroy(task);
    else
        _task_destroy(task);
}

void _task_switch_gate(interrupt_registers_t *regs)
{
    task_t *current = _current_task;
    task_t *target = _next_task;

    _task_destroy_deferred();

    if (!target || target->state != TASK_STATE_RUNNABLE)
        target = task_next(current);

    if (current)
        _task_state_save(current, regs);

    if (current && current->state == TASK_STATE_EXITING) {
        _task_remove_children(current);
        _task_unlink(current);
        if (!target || target == current)
            target = task_next(NULL);
        _task_defer_destroy(current);
    }

    if (!target) {
        target = &_task_list_head;
    }

    _current_task = target ? target : &_task_list_head;
    _next_task = NULL;

    gdt_tss_set_rsp0(_current_task->regs.rsp0);

    if (_current_task->regs.cr3)
        asm_write_cr3(_current_task->regs.cr3);

    _task_state_load(_current_task, regs);
}

void task_switching_init()
{
    memset(&_task_list_head, 0, sizeof(_task_list_head));
    _task_list_head.next = &_task_list_head;
    _task_list_head.user_mode = false;
    _task_list_head.state = TASK_STATE_RUNNABLE;
    _task_list_head.regs.cr3 = asm_read_cr3();
    _task_list_head.regs.cs = KERNEL_CODE_SELECTOR;
    _task_list_head.regs.ss = KERNEL_DATA_SELECTOR;
    _task_list_head.regs.rflags = DEFAULT_RFLAGS;
    _task_list_head.regs.rsp = asm_read_rsp();
    _task_list_head.regs.rsp0 = _task_list_head.regs.rsp;

    _task_list_head.regs.fx_state = kmalloc(512 + 16);
    if (_task_list_head.regs.fx_state) {
        uintptr_t fx_addr = (uintptr_t) _task_list_head.regs.fx_state;
        _task_list_head.regs.fx_state_aligned = (void *) ((fx_addr + 15) & ~((uintptr_t) 0xF));
        memset(_task_list_head.regs.fx_state_aligned, 0, 512);
        uint8_t *fx_region = (uint8_t *) _task_list_head.regs.fx_state_aligned;
        *((uint16_t *) &fx_region[0]) = 0x037F;  /* FCW */
        *((uint32_t *) &fx_region[24]) = 0x1F80; /* MXCSR */
    }

    _task_list_tail = &_task_list_head;
    _current_task = &_task_list_head;
    _next_task = NULL;

    idt_set_gate(0x30, _task_switch_gate_stub, IDT_TYPE_SOFTWARE);
}

void task_switch(task_t *task)
{
    if (!debug_assert(_current_task))
        return;

    if (!task)
        task = _current_task->next ? _current_task->next : &_task_list_head;

    if (task == _current_task)
        return;

    _next_task = task;
    __asm__ volatile("int $0x30");
}

void task_set_state(task_t *task, task_lifecycle_state_t state)
{
    if (!debug_assert(task))
        return;

    task->state = state;
}

task_t *task_next(task_t *task)
{
    task_t *start = task ? task : &_task_list_head;
    task_t *cursor = start;

    do {
        cursor = cursor->next ? cursor->next : &_task_list_head;
        if (cursor != &_task_list_head && cursor != start
            && cursor->state == TASK_STATE_RUNNABLE)
            return cursor;
    } while (cursor != start);

    return &_task_list_head;
}

void task_mark_exiting(task_t *task)
{
    if (!debug_assert(task))
        return;
    if (!debug_assert(task != &_task_list_head))
        return;

    _task_remove_children(task);
    task->state = TASK_STATE_EXITING;
}

task_t *task_idle()
{
    return &_task_list_head;
}
