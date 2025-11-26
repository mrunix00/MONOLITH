/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/usermode/task.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/debug.h>
#include <kernel/fs/vfs.h>
#include <kernel/input/ps2_keyboard.h>
#include <kernel/input/ps2_mouse.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/vmm.h>
#include <kernel/terminal/kshell.h>
#include <kernel/terminal/terminal.h>
#include <kernel/usermode/syscall.h>
#include <stdint.h>

extern void _syscall_handler();

void syscalls_init()
{
    idt_set_gate(0x80, _syscall_handler, IDT_TYPE_SOFTWARE);
}

void sys_hello()
{
    kprintf("\nSyscalls are working!");
    debug_log_fmt("[*] Syscalls are working!\n");
}

extern struct limine_framebuffer_request framebuffer_request;
int sys_request_fb(void *fb_info)
{
    if (framebuffer_request.response->framebuffer_count < 1)
        return -1;
    size_t width = framebuffer_request.response->framebuffers[0]->width;
    size_t height = framebuffer_request.response->framebuffers[0]->height;
    void *lhfb = vmm_get_lhdm_addr(framebuffer_request.response->framebuffers[0]->address);
    void *hhfb = framebuffer_request.response->framebuffers[0]->address;
    vmm_map_range(
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

int sys_register_mouse_handler(ps2_mouse_event_handler_t handler)
{
    ps2_mouse_register_event_handler(handler);
    return 0;
}

int sys_register_keyboard_handler(keyboard_event_handler_t handler)
{
    ps2_keyboard_register_event_handler(handler);
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
