/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/shm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/pipe.h>
#include <kernel/tasking/syscall.h>
#include <kernel/timer.h>
#include <shared/include/monolith/sys/syscall.h>

#define TASK_CREATE_MAX_ARGS 256
#define TASK_CREATE_MAX_INHERITED_RDS 256

static void _free_task_create_argv(const char **argv, int argc)
{
    if (argv == NULL)
        return;

    for (int i = 0; i < argc; i++) {
        if (argv[i] != NULL)
            kfree((void *) argv[i]);
    }
    kfree(argv);
}

static rsrc_status_t _alloc_task_handle(task_t *task, rsrc_t *resource)
{
    if (task == NULL)
        return RSRC_ERROR;
    if (resource == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    int handle = rsmgr_handle_table_alloc(&task->handle_table, resource);
    return handle < 0 ? RSRC_ERROR_NO_MEMORY : (rsrc_status_t) handle;
}

rsrc_status_t sys_rsrc_open(const char *path)
{
    if (!syscall_user_ptr_range(path, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_t *resource = rsmgr_open(path);
    if (resource == NULL)
        return RSRC_ERROR_NOT_FOUND;

    return _alloc_task_handle(current, resource);
}

rsrc_status_t sys_rsrc_close(int fd)
{
    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    return rsmgr_handle_table_close(&current->handle_table, fd);
}

rsrc_status_t sys_rsrc_dup(int fd)
{
    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    int new_fd = rsmgr_handle_table_alloc(&current->handle_table, entry->resource);
    if (new_fd < 0)
        return RSRC_ERROR_NO_MEMORY;

    rsrc_handle_entry_t *new_entry = rsmgr_handle_table_get(&current->handle_table, new_fd);
    if (new_entry == NULL) {
        rsmgr_handle_table_close(&current->handle_table, new_fd);
        return RSRC_ERROR;
    }

    new_entry->offset = entry->offset;
    return new_fd;
}

rsrc_status_t sys_rsrc_read(int fd, void *buffer, uint64_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;
    uint64_t bytes_read = 0;
    rsrc_status_t result = rsmgr_read(entry->resource, &entry->offset, buffer, size, &bytes_read);
    if (result != RSRC_STATUS_OK)
        return result;

    entry->offset += bytes_read;
    return bytes_read;
}

rsrc_status_t sys_rsrc_write(int fd, const void *buffer, uint64_t size)
{
    if (!syscall_user_ptr_range(buffer, size))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;
    uint64_t bytes_written = 0;
    rsrc_status_t result
        = rsmgr_write(entry->resource, &entry->offset, buffer, size, &bytes_written);
    if (result != RSRC_STATUS_OK)
        return result;

    entry->offset += bytes_written;
    return bytes_written;
}

rsrc_status_t sys_rsrc_list(int fd, uint64_t cursor, void *buffer, uint64_t buffer_len)
{
    if (!syscall_user_ptr_range(buffer, buffer_len))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    uint64_t bytes_written = 0;
    uint64_t next_cursor = 0;
    rsrc_status_t result
        = rsmgr_list(entry->resource, NULL, cursor, buffer, buffer_len, &next_cursor, &bytes_written);
    if (result != RSRC_STATUS_OK)
        return result;

    return bytes_written;
}

rsrc_status_t sys_rsrc_remove(int parent_fd, const char *name)
{
    if (!syscall_user_ptr_range(name, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, parent_fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    return rsmgr_remove(entry->resource, NULL, name);
}

rsrc_status_t sys_rsrc_lookup(int parent_fd, const char *path)
{
    if (!syscall_user_ptr_range(path, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, parent_fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    rsrc_t *child = rsmgr_lookup(entry->resource, path);
    if (child == NULL)
        return RSRC_ERROR_NOT_FOUND;
    return _alloc_task_handle(current, child);
}

rsrc_status_t sys_file_create(const char *path)
{
    if (!syscall_user_ptr_range(path, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_t *resource = NULL;
    rsrc_status_t result = rsmgr_create_file(path, &resource);
    if (result != RSRC_STATUS_OK)
        return result;
    if (resource == NULL)
        return RSRC_ERROR;

    return _alloc_task_handle(current, resource);
}

rsrc_status_t sys_ipc_channel_create(const char *name)
{
    if (name != NULL && !syscall_user_ptr_range(name, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_t *resource = NULL;
    rsrc_status_t result = ipc_channel_create(name, &resource);
    if (result != RSRC_STATUS_OK)
        return result;
    if (resource == NULL)
        return RSRC_ERROR;

    return _alloc_task_handle(current, resource);
}

rsrc_status_t sys_shm_create(const char *name, size_t size)
{
    if (name != NULL && !syscall_user_ptr_range(name, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_t *resource = NULL;
    rsrc_status_t result = shm_create(name, size, &resource);
    if (result != RSRC_STATUS_OK)
        return result;
    if (resource == NULL)
        return RSRC_ERROR;

    return _alloc_task_handle(current, resource);
}

rsrc_status_t sys_pipe_create(const char *name)
{
    if (name != NULL && !syscall_user_ptr_range(name, 1))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_t *resource = NULL;
    rsrc_status_t result = pipe_create(name, &resource);
    if (result != RSRC_STATUS_OK)
        return result;
    if (resource == NULL)
        return RSRC_ERROR;

    return _alloc_task_handle(current, resource);
}

rsrc_status_t sys_rsrc_describe(int fd, rsrc_info_t *out_info)
{
    if (!syscall_user_ptr_range(out_info, sizeof(*out_info)))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    return rsmgr_describe(entry->resource, out_info);
}

rsrc_status_t sys_rsrc_seek(int fd, int offset, int whence)
{
    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    uint64_t target = 0;
    rsrc_status_t result
        = rsmgr_seek(entry->resource, &entry->offset, offset, (uint64_t) whence, &target);
    if (result != RSRC_STATUS_OK)
        return result;

    entry->offset = target;
    if (entry->offset > (uint64_t) INT32_MAX)
        return INT32_MAX;
    return (int) entry->offset;
}

rsrc_status_t sys_rsrc_control(int fd, uint64_t command_id, void *buffer, uint64_t buffer_len)
{
    if (buffer != NULL && !syscall_user_ptr_range(buffer, buffer_len))
        return RSRC_ERROR_INVALID_ARGUMENT;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    uint64_t bytes_written = 0;
    rsrc_status_t result = rsmgr_control(
        entry->resource, &entry->offset, command_id, buffer, buffer_len, &bytes_written);
    if (result != RSRC_STATUS_OK)
        return result;

    if (bytes_written > 0)
        return (rsrc_status_t) bytes_written;
    return result;
}

rsrc_status_t sys_rsrc_mmap(int fd, uint64_t offset, uint64_t length, uint64_t prot)
{
    task_t *current = task_get_current();
    if (length == 0)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (current == NULL)
        return RSRC_ERROR;

    rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, fd);
    if (entry == NULL || entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    uint64_t address = 0;
    rsrc_status_t result
        = rsmgr_mmap(entry->resource, &entry->offset, offset, length, prot, &address);
    if (result != RSRC_STATUS_OK)
        return result;
    return (rsrc_status_t) address;
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

int sys_task_create(int argc, const char **argv, const int *inherit_rds, int inherit_rd_count)
{
    if (argc < 1 || !syscall_user_ptr_range(argv, argc * sizeof(const char *)))
        return -1;
    if (inherit_rd_count < 0 || inherit_rd_count > TASK_CREATE_MAX_INHERITED_RDS)
        return -1;
    if (inherit_rd_count > 0
        && !syscall_user_ptr_range(inherit_rds, inherit_rd_count * sizeof(int)))
        return -1;

    task_t *current = task_get_current();
    if (current == NULL)
        return -1;

    if (argc > TASK_CREATE_MAX_ARGS)
        argc = TASK_CREATE_MAX_ARGS;

    for (int i = 0; i < inherit_rd_count; i++) {
        int rd = inherit_rds[i];
        if (rsmgr_handle_table_get(&current->handle_table, rd) == NULL)
            return -1;
    }

    const char **k_argv = (const char **) kmalloc(sizeof(const char *) * argc);
    if (!k_argv)
        return -1;
    memset(k_argv, 0, sizeof(const char *) * argc);

    for (int i = 0; i < argc; i++) {
        const char *user_str = argv[i];
        if (!syscall_user_ptr_range(user_str, 1)) {
            _free_task_create_argv(k_argv, argc);
            return -1;
        }

        /* Copy the string: read up to 256 bytes to find the null terminator */
        size_t max_len = 256;
        const char *base = user_str;
        size_t len = 0;
        while (len < max_len) {
            uint8_t byte;
            if (!syscall_user_ptr_range(user_str + len, 1))
                break;
            byte = *((const uint8_t *) user_str + len);
            if (byte == 0)
                break;
            len++;
        }

        char *k_str = (char *) kmalloc(len + 1);
        if (!k_str) {
            _free_task_create_argv(k_argv, argc);
            return -1;
        }
        memcpy(k_str, base, len);
        k_str[len] = '\0';
        k_argv[i] = k_str;
    }

    task_t *child = load_exec(k_argv[0], current, argc, k_argv);
    if (child == NULL) {
        debug_log_fmt("Failed to load %s!\n", k_argv[0]);
        _free_task_create_argv(k_argv, argc);
        return -1;
    }

    for (int i = 0; i < inherit_rd_count; i++) {
        int rd = inherit_rds[i];
        rsrc_handle_entry_t *entry = rsmgr_handle_table_get(&current->handle_table, rd);
        if (rsmgr_handle_table_get(&child->handle_table, rd) != NULL)
            continue;
        if (entry == NULL
            || rsmgr_handle_table_inherit(&child->handle_table, rd, entry) != RSRC_STATUS_OK) {
            task_remove(child);
            _free_task_create_argv(k_argv, argc);
            return -1;
        }
    }

    rsrc_t *task_resource = child->resource_node == NULL ? NULL : child->resource_node->resource;
    if (task_resource == NULL) {
        task_remove(child);
        _free_task_create_argv(k_argv, argc);
        return -1;
    }

    int handle = _alloc_task_handle(current, task_resource);
    if (handle < 0) {
        task_remove(child);
        _free_task_create_argv(k_argv, argc);
        return -1;
    }

    _free_task_create_argv(k_argv, argc);
    return handle;
}

void syscalls_task_cleanup(task_t *task)
{
    if (!task)
        return;

    rsmgr_handle_table_destroy(&task->handle_table);
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

long syscall_dispatch(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    switch (num) {
    case SYSCALL_EXIT:
        return sys_exit();
    case SYSCALL_SLEEP:
        sys_sleep((uint64_t) arg1);
        return 0;
    case SYSCALL_GET_TICKS:
        return (long) sys_get_ticks();
    case SYSCALL_ALLOC_PAGES:
        return (long) sys_alloc_pages((size_t) arg1, (uint64_t) arg2);
    case SYSCALL_UNMAP_PAGES:
        return sys_unmap_pages((void *) arg1, (size_t) arg2);
    case SYSCALL_RSRC_OPEN:
        return sys_rsrc_open((const char *) arg1);
    case SYSCALL_RSRC_CLOSE:
        return sys_rsrc_close((int) arg1);
    case SYSCALL_RSRC_DUP:
        return sys_rsrc_dup((int) arg1);
    case SYSCALL_RSRC_LIST:
        return sys_rsrc_list((int) arg1, (uint64_t) arg2, (void *) arg3, (uint64_t) arg4);
    case SYSCALL_RSRC_READ:
        return sys_rsrc_read((int) arg1, (void *) arg2, (uint64_t) arg3);
    case SYSCALL_RSRC_WRITE:
        return sys_rsrc_write((int) arg1, (const void *) arg2, (uint64_t) arg3);
    case SYSCALL_RSRC_REMOVE:
        return sys_rsrc_remove((int) arg1, (const char *) arg2);
    case SYSCALL_RSRC_LOOKUP:
        return sys_rsrc_lookup((int) arg1, (const char *) arg2);
    case SYSCALL_RSRC_DESCRIBE:
        return sys_rsrc_describe((int) arg1, (rsrc_info_t *) arg2);
    case SYSCALL_RSRC_SEEK:
        return sys_rsrc_seek((int) arg1, (int) arg2, (int) arg3);
    case SYSCALL_RSRC_CONTROL:
        return sys_rsrc_control((int) arg1, (uint64_t) arg2, (void *) arg3, (uint64_t) arg4);
    case SYSCALL_RSRC_MMAP:
        return sys_rsrc_mmap((int) arg1, (uint64_t) arg2, (uint64_t) arg3, (uint64_t) arg4);
    case SYSCALL_FILE_CREATE:
        return sys_file_create((const char *) arg1);
    case SYSCALL_IPC_CHANNEL_CREATE:
        return sys_ipc_channel_create((const char *) arg1);
    case SYSCALL_SHM_CREATE:
        return sys_shm_create((const char *) arg1, (size_t) arg2);
    case SYSCALL_TASK_CREATE:
        return sys_task_create((int) arg1, (const char **) arg2, (const int *) arg3, (int) arg4);
    case SYSCALL_PIPE_CREATE:
        return sys_pipe_create((const char *) arg1);
    default:
        return -1;
    }
}
