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

#define IA32_EFER 0xC0000080
#define IA32_STAR 0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084
#define IA32_EFER_SCE (1ULL << 0)
#define KERNEL_CODE_SELECTOR 0x08
#define USER_CODE_SELECTOR 0x1B

uintptr_t syscall_kernel_stack_top;

void syscalls_init()
{
    uint64_t efer = asm_read_msr(IA32_EFER);
    asm_write_msr(IA32_EFER, efer | IA32_EFER_SCE);
    asm_write_msr(
        IA32_STAR,
        ((uint64_t) (USER_CODE_SELECTOR & ~0x3) << 48) | ((uint64_t) KERNEL_CODE_SELECTOR << 32));
    asm_write_msr(IA32_LSTAR, (uint64_t) _syscall_handler);
    asm_write_msr(IA32_FMASK, 0);
}

#define USER_SPACE_START 0x0000000000001000ULL
#define USER_SPACE_END 0x0000800000000000ULL

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2
#define O_ACCMODE 0x3
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define FD_TABLE_INITIAL_CAPACITY 16

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

typedef struct
{
    uint32_t *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t alpha_mask_size;
    uint8_t alpha_mask_shift;
} _fb_info_t;
extern struct limine_framebuffer_request framebuffer_request;
int sys_request_fb(_fb_info_t *fb_info)
{
    task_t *current = task_get_current();
    if (!_user_ptr_range(fb_info, sizeof(_fb_info_t)))
        return -1;
    if (!current || (_fb_owner && _fb_owner != current)
        || framebuffer_request.response->framebuffer_count < 1)
        return -1;
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    /* Map framebuffer into the current task's address space */
    vmm_map_range(
        current->state.cr3,
        (uintptr_t) fb->address,
        (uintptr_t) vmm_get_lhdm_addr(fb->address),
        fb->pitch * fb->height,
        PTFLAG_P | PTFLAG_RW | PTFLAG_US | PTFLAG_PAT,
        true);

    *fb_info = (_fb_info_t){
        .address = fb->address,
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
    if (!_user_ptr_range(event, sizeof(*event)))
        return -1;
    if (!input_poll_event(event))
        return 1;
    return 0;
}

int sys_file_open(const char *path, int flags)
{
    if (!_user_ptr_range(path, 1))
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

int sys_file_read(int fd, void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_read(file, buffer, size);
}

int sys_file_write(int fd, const void *buffer, uint32_t size)
{
    if (!_user_ptr_range(buffer, size))
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
    if (!_user_ptr_range(buffer, size))
        return -1;

    task_t *current = task_get_current();
    file_t *file = _fd_get(current, fd);
    if (!file)
        return -1;

    return file_getdents(file, buffer, size);
}

int sys_file_getstats(int fd, file_stats_t *stats)
{
    if (!_user_ptr_range(stats, sizeof(*stats)))
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
    if (!_user_ptr_range(name, 1))
        return -1;
    return ipc_new_channel(task_get_current(), name);
}

channel_id_t sys_ipc_request_connection(const char *name)
{
    if (!_user_ptr_range(name, 1))
        return -1;
    return ipc_connect(task_get_current(), name);
}

int sys_ipc_wait_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_await_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_poll_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_poll_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_accept_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_accept_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_reject_connection(channel_id_t channel_id, connection_t *connection)
{
    if (!_user_ptr_range(connection, sizeof(*connection)))
        return -1;
    return ipc_reject_connection(task_get_current(), channel_id, connection);
}

int sys_ipc_send_to(channel_id_t channel_id, connection_t *connection, void *data, size_t size)
{
    if (!_user_ptr_range(connection, sizeof(*connection)) || !_user_ptr_range(data, size)) {
        return -1;
    }
    return ipc_send_to(task_get_current(), channel_id, connection, data, size);
}

int sys_ipc_send(channel_id_t channel_id, void *data, size_t size)
{
    if (!_user_ptr_range(data, size))
        return -1;
    return ipc_send(task_get_current(), channel_id, data, size);
}

int sys_ipc_release_shm(channel_id_t channel_id, void *addr)
{
    if (!_user_ptr_range(addr, 1))
        return -1;
    return ipc_release_shared_memory(task_get_current(), channel_id, addr);
}

void *sys_ipc_share_shm(channel_id_t channel_id, connection_t *cnx, void *owner_addr, size_t size)
{
    if (!_user_ptr_range(cnx, sizeof(*cnx)) || !_user_ptr_range(owner_addr, 1) || size == 0)
        return NULL;
    return ipc_share_memory(task_get_current(), channel_id, cnx, owner_addr, size);
}

int sys_unmap_pages(void *addr, size_t num_pages)
{
    if (!_user_ptr_range(addr, 1) || num_pages == 0)
        return -1;
    task_t *current = task_get_current();
    if (!current)
        return -1;
    return task_unmap(current, (uintptr_t) addr, num_pages, true);
}

int sys_ipc_receive(channel_id_t channel_id, connection_t *sender, void *data, size_t size)
{
    if (!_user_ptr_range(sender, sizeof(*sender)) || !_user_ptr_range(data, size))
        return -1;

    return ipc_receive(task_get_current(), channel_id, sender, data, size);
}

int sys_ipc_disconnect(channel_id_t channel_id)
{
    return ipc_disconnect(task_get_current(), channel_id);
}
