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
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task.h>
#include <kernel/timer.h>

extern void _syscall_handler();
static task_t *_fb_owner;

void syscalls_init()
{
    idt_set_gate(0x80, _syscall_handler, IDT_TYPE_SOFTWARE);
}

#define USER_SPACE_START 0x0000000000001000ULL
#define USER_SPACE_END 0x0000800000000000ULL

static bool _user_ptr_range(const void *ptr, size_t size)
{
    if (!ptr)
        return false;

    uintptr_t start = (uintptr_t) ptr;
    if (start < USER_SPACE_START || start >= USER_SPACE_END)
        return false;

    if (size == 0)
        return true;

    uintptr_t end = start + size;
    if (end < start)
        return false;

    return end <= USER_SPACE_END;
}

extern struct limine_framebuffer_request framebuffer_request;
int sys_request_fb(void *fb_info)
{
    task_t *current = task_get_current();
    if (!_user_ptr_range(fb_info, sizeof(void *) * 3))
        return -1;
    if (!current)
        return -1;
    if (_fb_owner && _fb_owner != current)
        return -1;
    if (framebuffer_request.response->framebuffer_count < 1)
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
    _fb_owner = current;
    return 0;
}

int sys_poll_input_event(input_event_t *event)
{
    if (!_user_ptr_range(event, sizeof(*event)))
        return -1;
    if (!input_poll_event(event))
        return 1;
    return 0;
}

void *sys_file_open(const char *path)
{
    if (!_user_ptr_range(path, 1))
        return NULL;
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
    if (!_user_ptr_range(path, 1))
        return -1;
    return file_create(path, type);
}

int sys_file_remove(const char *path)
{
    if (!_user_ptr_range(path, 1))
        return -1;
    return file_remove(path);
}

int sys_file_read(file_t *file, void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
        return -1;
    return file_read(file, buffer, size);
}

int sys_file_write(file_t *file, const void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
        return -1;
    return file_write(file, buffer, size);
}

int sys_file_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    return file_seek(file, offset, mode);
}

int sys_file_getdents(file_t *file, void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
        return -1;
    return file_getdents(file, buffer, size);
}

int sys_file_getstats(file_t *file, file_stats_t *stats)
{
    if (!_user_ptr_range(stats, sizeof(*stats)))
        return -1;
    return file_getstats(file, stats);
}

size_t sys_file_tell(file_t *file)
{
    return file_tell(file);
}

int sys_getdrives(void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
        return -1;
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

/* Find an unused virtual address region in the task's address space */
static uintptr_t _find_free_vaddr(task_t *task, size_t num_pages)
{
    size_t required_size = num_pages * PAGE_SIZE;
    uintptr_t candidate = USER_SPACE_START;

    if (task->memory.memblocks == NULL || task->memory.memblocks_count == 0)
        return candidate;

    /* First-fit search with rescan to avoid overlapping earlier blocks. */
    bool adjusted;
    do {
        adjusted = false;
        for (size_t i = 0; i < task->memory.memblocks_count; i++) {
            task_memblock_t *block = &task->memory.memblocks[i];
            uintptr_t block_end = block->virt_addr + block->page_count * PAGE_SIZE;

            /* If candidate overlaps with this block, move past it and rescan. */
            if (candidate < block_end && candidate + required_size > block->virt_addr) {
                candidate = block_end;
                adjusted = true;
                break;
            }
        }
    } while (adjusted);

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
    if (!_user_ptr_range(path, 1))
        return;
    if (load_elf(path) == NULL)
        debug_log_fmt("Failed to load %s!", path);
}

void sys_test(void)
{
    debug_log_fmt("Test\n");
}

void syscalls_task_cleanup(task_t *task)
{
    if (_fb_owner == task)
        _fb_owner = NULL;
}

int sys_ipc_new(const char *name)
{
    if (!_user_ptr_range(name, 1))
        return -1;
    return ipc_new_channel(task_get_current(), name);
}

int sys_ipc_request_connection(const char *name, channel_t *channel)
{
    if (!_user_ptr_range(name, 1) || !_user_ptr_range(channel, sizeof(*channel)))
        return -1;
    return ipc_connect(task_get_current(), name, channel);
}

int sys_ipc_wait_connection(channel_t *channel, connection_t *connection)
{
    if (!_user_ptr_range(channel, sizeof(*channel))
        || !_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_await_connection(task_get_current(), channel, connection);
}

int sys_ipc_accept_connection(channel_t *channel, connection_t *connection)
{
    if (!_user_ptr_range(channel, sizeof(*channel))
        || !_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_accept_connection(task_get_current(), channel, connection);
}

int sys_ipc_reject_connection(channel_t *channel, connection_t *connection)
{
    if (!_user_ptr_range(channel, sizeof(*channel))
        || !_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_reject_connection(task_get_current(), channel, connection);
}

int sys_ipc_send_to(channel_t *channel, connection_t *connection, void *data, size_t size)
{
    if (!_user_ptr_range(channel, sizeof(*channel))
        || !_user_ptr_range(connection, sizeof(*connection)) || !_user_ptr_range(data, size)) {
        return -1;
    }
    return ipc_send_to(task_get_current(), channel, connection, data, size);
}

int sys_ipc_send(channel_t *channel, void *data, size_t size)
{
    if (!_user_ptr_range(channel, sizeof(*channel)) || !_user_ptr_range(data, size))
        return -1;
    return ipc_send(task_get_current(), channel, data, size);
}

int sys_ipc_receive(channel_t *channel, connection_t *sender, void *data, size_t size)
{
    if (!_user_ptr_range(channel, sizeof(*channel)) || !_user_ptr_range(sender, sizeof(*sender))
        || !_user_ptr_range(data, size))
        return -1;

    return ipc_receive(task_get_current(), channel, sender, data, size);
}

int sys_ipc_disconnect(channel_t *channel)
{
    if (!_user_ptr_range(channel, sizeof(*channel)))
        return -1;
    return ipc_disconnect(task_get_current(), channel);
}
