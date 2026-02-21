/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/sse.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task.h>
#include <kernel/timer.h>
#include <stdint.h>

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

extern void _task_switch_gate_stub();

static void _task_state_save(task_t *task, interrupt_registers_t *regs);
static void _task_state_load(task_t *task, interrupt_registers_t *regs);
static void _task_unlink(task_t *task);
static void _task_destroy(task_t *task);

task_t *task_create(void *entry_point, const char *name, task_mode_t mode)
{
    task_t *task = (task_t *) kmalloc(sizeof(task_t));
    if (!task) {
        return NULL;
    }

    memset(task, 0, sizeof(task_t));
    task->user_mode = (mode == TASK_MODE_USER);
    task->state.rip = (uintptr_t) entry_point;
    task->state.rflags = DEFAULT_RFLAGS;
    task->state.cs = task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR;
    task->state.ss = task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR;
    task->quantum = DEFAULT_QUANTUM;

    task->state.fx_state = kmalloc(512 + 16);
    if (!task->state.fx_state) {
        kfree(task);
        return NULL;
    }
    uintptr_t fx_addr = (uintptr_t) task->state.fx_state;
    task->state.fx_state_aligned = (void *) ((fx_addr + 15) & ~((uintptr_t) 0xF));
    memset(task->state.fx_state_aligned, 0, 512);

    /* Initialize FPU state with defaults */
    uint8_t *fx_region = (uint8_t *) task->state.fx_state_aligned;
    *((uint16_t *) &fx_region[0]) = 0x037F;  /* FCW */
    *((uint32_t *) &fx_region[24]) = 0x1F80; /* MXCSR */

    task->stack_bottom = (uintptr_t) kmalloc(KERNEL_STACK_SIZE);
    if (!task->stack_bottom) {
        kfree(task->state.fx_state);
        kfree(task);
        return NULL;
    }
    task->state.rsp0 = task->stack_bottom + KERNEL_STACK_SIZE;

    if (task->user_mode) {
        task->state.cr3 = vmm_create_address_space();
        if (task->state.cr3 == 0) {
            kfree((void *) task->stack_bottom);
            kfree(task);
            return NULL;
        }
    } else {
        task->state.cr3 = vmm_get_kernel_cr3();
    }

    task->next = &_task_list_head;
    _task_list_tail->next = task;
    _task_list_tail = task;

    task->name = strdup(name);

    return task;
}

int task_map(
    task_t *task,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    size_t page_count,
    uintptr_t flags,
    bool release_on_exit)
{
    if (task->memory.memblocks == NULL) {
        task->memory.memblocks_count = 0;
        task->memory.memblocks_size = 16;
        task->memory.memblocks = (task_memblock_t *) kmalloc(
            sizeof(task_memblock_t) * task->memory.memblocks_size);
        if (!task->memory.memblocks) {
            return -1;
        }
    }

    if (task->memory.memblocks_count == task->memory.memblocks_size) {
        task_memblock_t *new_memblocks = (task_memblock_t *) krealloc(
            task->memory.memblocks, sizeof(task_memblock_t) * task->memory.memblocks_size * 2);
        if (!new_memblocks)
            return -1;
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
    vmm_map_range(task->state.cr3, virt_addr, phys_addr, page_count * PAGE_SIZE, flags, true);

    return 0;
}

task_t *task_get_current()
{
    return _current_task;
}

uintptr_t task_find_free_vaddr(task_t *task, size_t num_pages)
{
    if (!task || num_pages == 0)
        return 0;

    size_t required_size = num_pages * PAGE_SIZE;
    uintptr_t candidate = USER_SPACE_START;

    if (task->memory.memblocks == NULL || task->memory.memblocks_count == 0) {
        if (candidate < USER_SPACE_START || candidate + required_size > USER_SPACE_END)
            return 0;
        return candidate;
    }

    bool adjusted;
    do {
        adjusted = false;
        for (size_t i = 0; i < task->memory.memblocks_count; i++) {
            task_memblock_t *block = &task->memory.memblocks[i];
            uintptr_t block_end = block->virt_addr + block->page_count * PAGE_SIZE;

            if (candidate < block_end && candidate + required_size > block->virt_addr) {
                candidate = block_end;
                adjusted = true;
                break;
            }
        }
    } while (adjusted);

    if (candidate < USER_SPACE_START || candidate + required_size > USER_SPACE_END)
        return 0;

    return candidate;
}

task_t *task_find_by_id(uint64_t id)
{
    if (id == 0)
        return NULL;

    task_t *cursor = _task_list_head.next ? _task_list_head.next : &_task_list_head;
    while (cursor && cursor != &_task_list_head) {
        if (cursor->id == id)
            return cursor;
        cursor = cursor->next ? cursor->next : &_task_list_head;
    }

    return NULL;
}

int task_unmap(task_t *task, uintptr_t virt_addr, size_t page_count, bool release_on_exit)
{
    if (!task || page_count == 0)
        return -1;

    vmm_unmap_range(task->state.cr3, virt_addr, page_count * PAGE_SIZE, true);

    if (!task->memory.memblocks || task->memory.memblocks_count == 0)
        return 0;

    for (size_t i = 0; i < task->memory.memblocks_count; i++) {
        task_memblock_t *memblock = &task->memory.memblocks[i];
        if (memblock->virt_addr == virt_addr && memblock->page_count == page_count) {
            if (release_on_exit && memblock->phys_addr)
                pmm_free((void *) memblock->phys_addr, memblock->page_count);

            for (size_t j = i + 1; j < task->memory.memblocks_count; j++)
                task->memory.memblocks[j - 1] = task->memory.memblocks[j];
            task->memory.memblocks_count--;
            break;
        }
    }

    return 0;
}

void task_remove(task_t *task)
{
    if (!task || task == &_task_list_head)
        return;

    if (_current_task == task)
        _current_task = task->next ? task->next : &_task_list_head;

    _task_unlink(task);
    _task_destroy(task);
}

void _task_switch_gate(interrupt_registers_t *regs)
{
    task_t *current = _current_task;
    task_t *target = _next_task;

    if (!target) {
        target = task_next(current);
    }

    if (!target || target == current)
        target = task_idle();

    if (current) {
        _task_state_save(current, regs);
    }

    if (current && current->exiting) {
        task_t *exiting_task = current;
        _task_unlink(exiting_task);
        if (target == exiting_task || !target)
            target = task_next(NULL);
        _task_destroy(exiting_task);
    }

    if (!target) {
        target = &_task_list_head;
    }

    _current_task = target;
    _next_task = NULL;

    gdt_tss_set_rsp0(_current_task->state.rsp0);

    if (_current_task->state.cr3)
        asm_write_cr3(_current_task->state.cr3);

    _task_state_load(_current_task, regs);
}

void task_switching_init()
{
    memset(&_task_list_head, 0, sizeof(_task_list_head));
    _task_list_head.next = &_task_list_head;
    _task_list_head.user_mode = false;
    _task_list_head.exiting = false;
    _task_list_head.state.cr3 = asm_read_cr3();
    _task_list_head.state.cs = KERNEL_CODE_SELECTOR;
    _task_list_head.state.ss = KERNEL_DATA_SELECTOR;
    _task_list_head.state.rflags = DEFAULT_RFLAGS;
    _task_list_head.state.rsp = asm_read_rsp();
    _task_list_head.state.rsp0 = _task_list_head.state.rsp;

    _task_list_head.state.fx_state = kmalloc(512 + 16);
    if (_task_list_head.state.fx_state) {
        uintptr_t fx_addr = (uintptr_t) _task_list_head.state.fx_state;
        _task_list_head.state.fx_state_aligned = (void *) ((fx_addr + 15) & ~((uintptr_t) 0xF));
        memset(_task_list_head.state.fx_state_aligned, 0, 512);
        uint8_t *fx_region = (uint8_t *) _task_list_head.state.fx_state_aligned;
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
    if (!_current_task)
        return;

    if (!task)
        task = _current_task->next ? _current_task->next : &_task_list_head;

    if (task == _current_task)
        return;

    _next_task = task;
    __asm__ volatile("int $0x30");
}

task_t *task_next(task_t *task)
{
    task_t *start = task ? task : &_task_list_head;
    task_t *cursor = start;

    do {
        cursor = cursor->next ? cursor->next : &_task_list_head;
        if (!cursor)
            return &_task_list_head;

        if (cursor != &_task_list_head && cursor != start && !cursor->exiting)
            return cursor;
    } while (cursor != start);

    return &_task_list_head;
}

void task_mark_exiting(task_t *task)
{
    if (!task || task == &_task_list_head)
        return;
    task->exiting = true;
}

task_t *task_idle()
{
    return &_task_list_head;
}

static void _task_state_save(task_t *task, interrupt_registers_t *regs)
{
    if (!task || !regs)
        return;

    task->state.rax = regs->rax;
    task->state.rbx = regs->rbx;
    task->state.rcx = regs->rcx;
    task->state.rdx = regs->rdx;
    task->state.rsi = regs->rsi;
    task->state.rdi = regs->rdi;
    task->state.rbp = regs->rbp;
    task->state.r8 = regs->r8;
    task->state.r9 = regs->r9;
    task->state.r10 = regs->r10;
    task->state.r11 = regs->r11;
    task->state.r12 = regs->r12;
    task->state.r13 = regs->r13;
    task->state.r14 = regs->r14;
    task->state.r15 = regs->r15;
    task->state.rip = regs->rip;
    task->state.rsp = regs->rsp;
    task->state.rflags = regs->rflags ? regs->rflags : DEFAULT_RFLAGS;
    task->state.cr3 = asm_read_cr3();

    uint16_t cs = (uint16_t) regs->cs;
    uint16_t ss = (uint16_t) regs->ss;
    if (!cs)
        cs = task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR;
    if (!ss)
        ss = task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR;

    task->state.cs = cs;
    task->state.ss = ss;

    if (task->state.fx_state_aligned)
        sse_save(task->state.fx_state_aligned);
}

static void _task_state_load(task_t *task, interrupt_registers_t *regs)
{
    if (!task || !regs)
        return;

    regs->rax = task->state.rax;
    regs->rbx = task->state.rbx;
    regs->rcx = task->state.rcx;
    regs->rdx = task->state.rdx;
    regs->rsi = task->state.rsi;
    regs->rdi = task->state.rdi;
    regs->rbp = task->state.rbp;
    regs->r8 = task->state.r8;
    regs->r9 = task->state.r9;
    regs->r10 = task->state.r10;
    regs->r11 = task->state.r11;
    regs->r12 = task->state.r12;
    regs->r13 = task->state.r13;
    regs->r14 = task->state.r14;
    regs->r15 = task->state.r15;
    regs->rip = task->state.rip;
    regs->rsp = task->state.rsp;
    regs->rflags = task->state.rflags ? task->state.rflags : DEFAULT_RFLAGS;

    uint16_t cs = task->state.cs;
    uint16_t ss = task->state.ss;
    if (!cs)
        cs = task->user_mode ? USER_CODE_SELECTOR : KERNEL_CODE_SELECTOR;
    if (!ss)
        ss = task->user_mode ? USER_DATA_SELECTOR : KERNEL_DATA_SELECTOR;

    /* Ensure SS.RPL matches the CPL from CS */
    uint8_t cs_cpl = cs & 0x03;
    uint8_t ss_rpl = ss & 0x03;
    if (cs_cpl != ss_rpl)
        ss = cs_cpl == 0 ? KERNEL_DATA_SELECTOR : USER_DATA_SELECTOR;

    regs->cs = cs;
    regs->ss = ss;

    if (task->state.fx_state_aligned)
        sse_restore(task->state.fx_state_aligned);
}

static void _task_unlink(task_t *task)
{
    if (!task || task == &_task_list_head)
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

static void _task_destroy(task_t *task)
{
    if (!task || task == &_task_list_head)
        return;

    /* Check if already destroyed (idempotency protection) */
    if (task->state.cr3 == 0xDEADBEEF)
        return;

    syscalls_task_cleanup(task);
    ipc_task_cleanup(task);

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

    if (task->user_mode && task->state.cr3 != 0) {
        vmm_destroy_address_space(task->state.cr3);
        task->state.cr3 = 0;
    }

    if (task->state.fx_state) {
        kfree(task->state.fx_state);
        task->state.fx_state = NULL;
    }

    if (task->stack_bottom) {
        kfree((void *) task->stack_bottom);
        task->stack_bottom = 0;
    }

    if (task->name) {
        kfree(task->name);
        task->name = NULL;
    }

    /* Mark as destroyed to prevent double-free */
    task->state.cr3 = 0xDEADBEEF;
    kfree(task);
}
