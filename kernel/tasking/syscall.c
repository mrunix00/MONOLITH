/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/debug.h>
#include <kernel/framebuffer.h>
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
#include <shared/include/monolith/sys/syscall.h>

static task_t *_fb_owner;

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2
#define O_ACCMODE 0x3
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define FD_TABLE_INITIAL_CAPACITY 16

static int _fd_ensure_capacity(task_t *task, size_t min_capacity)
{
    if (!task)
        return -1;

    if (task->fd_capacity >= min_capacity)
        return 0;

    size_t new_capacity = task->fd_capacity ? task->fd_capacity : FD_TABLE_INITIAL_CAPACITY;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    size_t old_capacity = task->fd_capacity;
    size_t new_size = new_capacity * sizeof(file_t);
    size_t old_size = old_capacity * sizeof(file_t);

    file_t *new_table = task->fd_table ? krealloc(task->fd_table, new_size) : kmalloc(new_size);
    if (!new_table)
        return -1;

    if (new_size > old_size) {
        memset((uint8_t *) new_table + old_size, 0, new_size - old_size);
    }

    task->fd_table = new_table;
    task->fd_capacity = new_capacity;
    return 0;
}

static int _fd_alloc(task_t *task, const file_t *file)
{
    if (!task || !file)
        return -1;

    for (size_t i = 0; i < task->fd_capacity; i++) {
        if (task->fd_table[i].internal == NULL) {
            task->fd_table[i] = *file;
            if (i >= task->fd_count)
                task->fd_count = i + 1;
            return (int) i;
        }
    }

    size_t new_index = task->fd_capacity;
    if (_fd_ensure_capacity(task, new_index + 1) < 0)
        return -1;

    task->fd_table[new_index] = *file;
    task->fd_count = new_index + 1;
    return (int) new_index;
}

static file_t *_fd_get(task_t *task, int fd)
{
    if (!task || fd < 0 || (size_t) fd >= task->fd_capacity)
        return NULL;

    file_t *file = &task->fd_table[fd];
    if (file->internal == NULL)
        return NULL;

    return file;
}

static int _fd_close(task_t *task, int fd)
{
    if (!task || fd < 0 || (size_t) fd >= task->fd_capacity)
        return -1;

    file_t *file = &task->fd_table[fd];
    if (file->internal == NULL)
        return -1;

    *file = (file_t){0};

    while (task->fd_count > 0) {
        size_t last = task->fd_count - 1;
        if (task->fd_table[last].internal != NULL)
            break;
        task->fd_count--;
    }

    return 0;
}

int sys_request_fb(framebuffer_t *fb_info)
{
    task_t *current = task_get_current();
    if (!syscall_user_ptr_range(fb_info, sizeof(framebuffer_t)))
        return -1;
    if (!current || _fb_owner != NULL)
        return -1;
    const framebuffer_t *fb = get_framebuffer(0);

    size_t page_count = PAGE_UP((size_t) fb->pitch * (size_t) fb->height) / PAGE_SIZE;
    uintptr_t virt_addr = task_find_free_vaddr(current, page_count);
    if (virt_addr == 0)
        return -1;

    task_map(
        current,
        virt_addr,
        (uintptr_t) vmm_get_lhdm_addr(fb->address),
        page_count,
        PTFLAG_P | PTFLAG_RW | PTFLAG_US | PTFLAG_PAT,
        true);

    *fb_info = (framebuffer_t){
        .address = (void *) virt_addr,
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .bpp = fb->bpp,
        .memory_model = fb->memory_model,
        .red_mask_size = fb->red_mask_size,
        .red_mask_shift = fb->red_mask_shift,
        .green_mask_size = fb->green_mask_size,
        .green_mask_shift = fb->green_mask_shift,
        .blue_mask_size = fb->blue_mask_size,
        .blue_mask_shift = fb->blue_mask_shift,
    };

    return 0;
}

int sys_poll_input_event(input_event_t *event)
{
    if (!syscall_user_ptr_range(event, sizeof(*event)))
        return -1;
    if (!input_poll_event(event))
        return 1;
    return 0;
}

int sys_file_open(const char *path, int flags)
{
    if (!syscall_user_ptr_range(path, 1))
        return -1;

    int accmode = flags & O_ACCMODE;
    if (accmode != O_RDONLY && accmode != O_WRONLY && accmode != O_RDWR)
        return -1;
    if (flags & ~(O_ACCMODE | O_CREAT | O_TRUNC))
        return -1;
    if (flags & O_TRUNC)
        return -1;

    if (flags & O_CREAT) {
        file_create(path, FILE);
    }

    task_t *current = task_get_current();
    if (!current)
        return -1;

    file_t file = file_open(path);
    if (file.internal == NULL)
        return -1;

    return _fd_alloc(current, &file);
}

int sys_file_close(int fd)
{
    task_t *current = task_get_current();
    if (!current)
        return -1;

    return _fd_close(current, fd);
}

int sys_file_create(const char *path, file_type_t type)
{
    if (!syscall_user_ptr_range(path, 1))
        return -1;
    return file_create(path, type);
}

int sys_file_remove(const char *path)
{
    if (!syscall_user_ptr_range(path, 1))
        return -1;
    return file_remove(path);
}

int sys_file_read(int fd, void *buffer, uint32_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_read(file, buffer, size);
}

int sys_file_write(int fd, const void *buffer, uint32_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_write(file, buffer, size);
}

int sys_file_seek(int fd, size_t offset, seek_mode_t mode)
{
    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_seek(file, offset, mode);
}

int sys_file_getdents(int fd, void *buffer, uint32_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_getdents(file, buffer, size);
}

int sys_file_getstats(int fd, file_stats_t *stats)
{
    if (!syscall_user_ptr_range(stats, sizeof(*stats)))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_getstats(file, stats);
}

size_t sys_file_tell(int fd)
{
    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return (size_t) -1;

    return file_tell(file);
}

int sys_getdrives(void *buffer, uint32_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return -1;
    return vfs_getdrives(buffer, size);
}

int sys_exit()
{
    task_t *current = task_get_current();
    if (!current)
        return -1;

    task_mark_exiting(current);
    task_t *next = task_next(current);

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

    uintptr_t virt_addr = task_find_free_vaddr(current, num_pages);
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
    if (!syscall_user_ptr_range(path, 1))
        return;
    if (load_elf_for_parent(path, task_get_current()) == NULL)
        debug_log_fmt("Failed to load %s!", path);
}

void syscalls_task_cleanup(task_t *task)
{
    if (_fb_owner == task)
        _fb_owner = NULL;

    if (!task)
        return;

    if (task->fd_table) {
        kfree(task->fd_table);
        task->fd_table = NULL;
    }
    task->fd_count = 0;
    task->fd_capacity = 0;
}

channel_id_t sys_ipc_new(const char *name)
{
    if (!syscall_user_ptr_range(name, 1))
        return -1;
    return ipc_new_channel(task_get_current(), name);
}

channel_id_t sys_ipc_request_connection(const char *name)
{
    if (!syscall_user_ptr_range(name, 1))
        return -1;
    return ipc_connect(task_get_current(), name);
}

int sys_ipc_wait_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!syscall_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_await_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_poll_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!syscall_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_poll_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_accept_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!syscall_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_accept_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_reject_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!syscall_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_reject_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_send_to(channel_id_t channel_id, connection_t *connection, void *data, size_t size)
{
    if (!syscall_user_ptr_range(connection, sizeof(*connection))
        || !syscall_user_ptr_range(data, size)) {
        return -1;
    }
    return ipc_send_to(task_get_current(), channel_id, connection, data, size);
}

int sys_ipc_send(channel_id_t channel_id, void *data, size_t size)
{
    if (!syscall_user_ptr_range(data, size))
        return -1;
    return ipc_send(task_get_current(), channel_id, data, size);
}

int sys_ipc_release_shm(channel_id_t channel_id, void *addr)
{
    if (!syscall_user_ptr_range(addr, 1))
        return -1;
    return ipc_release_shared_memory(task_get_current(), channel_id, addr);
}

void *sys_ipc_share_shm(channel_id_t channel_id, connection_t *cnx, void *owner_addr, size_t size)
{
    if (!syscall_user_ptr_range(cnx, sizeof(*cnx)) || !syscall_user_ptr_range(owner_addr, 1)
        || size == 0)
        return NULL;
    return ipc_share_memory(task_get_current(), channel_id, cnx, owner_addr, size);
}

int sys_unmap_pages(void *addr, size_t num_pages)
{
    if (!syscall_user_ptr_range(addr, 1) || num_pages == 0)
        return -1;
    task_t *current = task_get_current();
    if (!current)
        return -1;
    return task_unmap(current, (uintptr_t) addr, num_pages, true);
}

int sys_ipc_receive(channel_id_t channel_id, connection_t *sender, void *data, size_t size)
{
    if (!syscall_user_ptr_range(sender, sizeof(*sender)) || !syscall_user_ptr_range(data, size))
        return -1;

    return ipc_receive(task_get_current(), channel_id, sender, data, size);
}

int sys_ipc_disconnect(channel_id_t channel_id)
{
    return ipc_disconnect(task_get_current(), channel_id);
}

long syscall_dispatch(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    switch (num) {
    case SYSCALL_EXIT:
        return sys_exit();
    case SYSCALL_REQUEST_FB:
        return sys_request_fb((framebuffer_t *) arg1);
    case SYSCALL_POLL_INPUT_EVENT:
        return sys_poll_input_event((input_event_t *) arg1);
    case SYSCALL_FILE_OPEN:
        return sys_file_open((const char *) arg1, (int) arg2);
    case SYSCALL_FILE_CLOSE:
        return sys_file_close((int) arg1);
    case SYSCALL_FILE_CREATE:
        return sys_file_create((const char *) arg1, (file_type_t) arg2);
    case SYSCALL_FILE_REMOVE:
        return sys_file_remove((const char *) arg1);
    case SYSCALL_FILE_READ:
        return sys_file_read((int) arg1, (void *) arg2, (uint32_t) arg3);
    case SYSCALL_FILE_WRITE:
        return sys_file_write((int) arg1, (const void *) arg2, (uint32_t) arg3);
    case SYSCALL_FILE_SEEK:
        return sys_file_seek((int) arg1, (size_t) arg2, (seek_mode_t) arg3);
    case SYSCALL_FILE_GETDENTS:
        return sys_file_getdents((int) arg1, (void *) arg2, (uint32_t) arg3);
    case SYSCALL_FILE_GETSTATS:
        return sys_file_getstats((int) arg1, (file_stats_t *) arg2);
    case SYSCALL_FILE_TELL:
        return (long) sys_file_tell((int) arg1);
    case SYSCALL_GETDRIVES:
        return sys_getdrives((void *) arg1, (uint32_t) arg2);
    case SYSCALL_SLEEP:
        sys_sleep((uint64_t) arg1);
        return 0;
    case SYSCALL_GET_TICKS:
        return (long) sys_get_ticks();
    case SYSCALL_ALLOC_PAGES:
        return (long) sys_alloc_pages((size_t) arg1, (uint64_t) arg2);
    case SYSCALL_SPAWN_TASK:
        sys_spawn_task((const char *) arg1);
        return 0;
    case SYSCALL_IPC_NEW:
        return sys_ipc_new((const char *) arg1);
    case SYSCALL_IPC_REQUEST_CONNECTION:
        return sys_ipc_request_connection((const char *) arg1);
    case SYSCALL_IPC_WAIT_CONNECTION:
        return sys_ipc_wait_connection((channel_id_t) arg1, (connection_t *) arg2);
    case SYSCALL_IPC_ACCEPT_CONNECTION:
        return sys_ipc_accept_connection((channel_id_t) arg1, (connection_t *) arg2);
    case SYSCALL_IPC_REJECT_CONNECTION:
        return sys_ipc_reject_connection((channel_id_t) arg1, (connection_t *) arg2);
    case SYSCALL_IPC_SEND:
        return sys_ipc_send((channel_id_t) arg1, (void *) arg2, (size_t) arg3);
    case SYSCALL_IPC_RECEIVE_ALL:
        return sys_ipc_receive(
            (channel_id_t) arg1, (connection_t *) arg2, (void *) arg3, (size_t) arg4);
    case SYSCALL_IPC_DISCONNECT:
        return sys_ipc_disconnect((channel_id_t) arg1);
    case SYSCALL_IPC_SEND_TO:
        return sys_ipc_send_to(
            (channel_id_t) arg1, (connection_t *) arg2, (void *) arg3, (size_t) arg4);
    case SYSCALL_IPC_RELEASE_SHM:
        return sys_ipc_release_shm((channel_id_t) arg1, (void *) arg2);
    case SYSCALL_IPC_SHARE_SHM:
        return (long) sys_ipc_share_shm(
            (channel_id_t) arg1, (connection_t *) arg2, (void *) arg3, (size_t) arg4);
    case SYSCALL_UNMAP_PAGES:
        return sys_unmap_pages((void *) arg1, (size_t) arg2);
    case SYSCALL_IPC_POLL_CONNECTION:
        return sys_ipc_poll_connection((channel_id_t) arg1, (connection_t *) arg2);
    default:
        return -1;
    }
}
