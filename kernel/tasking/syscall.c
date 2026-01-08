/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/debug.h>
#include <kernel/fs/vfs.h>
#include <kernel/input/input_events.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task.h>
#include <kernel/timer.h>

extern void _syscall_handler();

void syscalls_init()
{
    idt_set_gate(0x80, _syscall_handler, IDT_TYPE_SOFTWARE);
}

extern struct limine_framebuffer_request framebuffer_request;
int sys_request_fb(void *fb_info)
{
    if (framebuffer_request.response->framebuffer_count < 1)
        return -1;
    task_t *current = task_get_current();
    if (!current)
        return -1;
    size_t width = framebuffer_request.response->framebuffers[0]->width;
    size_t height = framebuffer_request.response->framebuffers[0]->height;
    void *lhfb = vmm_get_lhdm_addr(framebuffer_request.response->framebuffers[0]->address);
    void *hhfb = framebuffer_request.response->framebuffers[0]->address;
    /* Map framebuffer into the current task's address space */
    vmm_map_range(
        current->state.cr3,
        (uintptr_t) hhfb,
        (uintptr_t) lhfb,
        width * height * 4,
        PTFLAG_P | PTFLAG_RW | PTFLAG_US | PTFLAG_PAT,
        true);
    memcpy(fb_info, &hhfb, sizeof(void *));
    memcpy(fb_info + sizeof(void *), &width, sizeof(uint64_t));
    memcpy(fb_info + 2 * sizeof(void *), &height, sizeof(uint64_t));
    return 0;
}

int sys_poll_input_event(input_event_t *event)
{
    if (!event)
        return -1;
    if (!input_poll_event(event))
        return 1;
    return 0;
}

void *sys_file_open(const char *path)
{
    file_t *file = kmalloc(sizeof(file_t));
    if (!file) {
        return NULL;
    }
    *file = file_open(path);
    return file;
}

void sys_file_close(void *file)
{
    kfree((file_t *) file);
}

int sys_file_create(const char *path, file_type_t type)
{
    return file_create(path, type);
}

int sys_file_remove(const char *path)
{
    return file_remove(path);
}

int sys_file_read(file_t *file, void *buffer, uint32_t size)
{
    return file_read(file, buffer, size);
}

int sys_file_write(file_t *file, const void *buffer, uint32_t size)
{
    return file_write(file, buffer, size);
}

int sys_file_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    return file_seek(file, offset, mode);
}

int sys_file_getdents(file_t *file, void *buffer, uint32_t size)
{
    return file_getdents(file, buffer, size);
}

int sys_file_getstats(file_t *file, file_stats_t *stats)
{
    return file_getstats(file, stats);
}

size_t sys_file_tell(file_t *file)
{
    return file_tell(file);
}

int sys_getdrives(void *buffer, uint32_t size)
{
    return vfs_getdrives(buffer, size);
}

int sys_exit()
{
    task_t *current = task_get_current();
    if (!current)
        return -1;

    task_t *next = task_next(current);
    task_mark_exiting(current);

    if (!next || next == current)
        next = task_next(NULL);

    if (!next || next == current)
        next = task_idle();

    task_switch(next);

    __builtin_unreachable();
    return 0;
}

void sys_sleep(uint64_t ms)
{
    sleep(ms);
}

uint64_t sys_get_ticks()
{
    return timer_get_ticks();
}

#define USER_SPACE_START 0x0000000000001000ULL
#define USER_SPACE_END 0x0000800000000000ULL

/* Find an unused virtual address region in the task's address space */
static uintptr_t _find_free_vaddr(task_t *task, size_t num_pages)
{
    size_t required_size = num_pages * PAGE_SIZE;
    uintptr_t candidate = USER_SPACE_START;

    if (task->memory.memblocks == NULL || task->memory.memblocks_count == 0)
        return candidate;

    /* First-fit search */
    for (size_t i = 0; i < task->memory.memblocks_count; i++) {
        task_memblock_t *block = &task->memory.memblocks[i];
        uintptr_t block_end = block->virt_addr + block->page_count * PAGE_SIZE;

        /* If candidate overlaps with this block, move past it */
        if (candidate < block_end && candidate + required_size > block->virt_addr)
            candidate = block_end;
    }

    return candidate;
}

void *sys_alloc_pages(size_t num_pages, uint64_t flags)
{
    if (num_pages == 0)
        return NULL;

    task_t *current = task_get_current();
    if (!current)
        return NULL;

    void *phys_mem = pmm_alloc(num_pages);
    if (!phys_mem)
        return NULL;

    uintptr_t virt_addr = _find_free_vaddr(current, num_pages);
    if (virt_addr == 0) {
        pmm_free(phys_mem, num_pages);
        return NULL;
    }

    uint64_t pt_flags = PTFLAG_P | PTFLAG_US;
    if (flags & ALLOC_PAGES_FLAG_RW)
        pt_flags |= PTFLAG_RW;
    if (!(flags & ALLOC_PAGES_FLAG_EXEC))
        pt_flags |= PTFLAG_XD;

    if (task_map(current, virt_addr, (uintptr_t) phys_mem, num_pages, pt_flags, true) < 0) {
        pmm_free(phys_mem, num_pages);
        return NULL;
    }

    memset(vmm_get_hhdm_addr(phys_mem), 0, num_pages * PAGE_SIZE);
    return (void *) virt_addr;
}

void sys_spawn_task(const char *path)
{
    file_t file = file_open(path);
    if (file.internal == NULL) {
        debug_log_fmt("[-] Failed to open %s executable\n", path);
        return;
    }

    if (load_elf(&file) < 0) {
        debug_log_fmt("Failed to load %s!", path);
        return;
    }
}

void sys_test(void)
{
    debug_log_fmt("Test\n");
}
