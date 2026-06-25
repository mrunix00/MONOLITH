/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <resource.h>
#include <unistd.h>

rsrc_handle_t rsmgr_open(const char *path)
{
    return (rsrc_handle_t) syscall1(SYSCALL_RSRC_OPEN, (long) path);
}

int rsmgr_close(rsrc_handle_t handle)
{
    return (int) syscall1(SYSCALL_RSRC_CLOSE, (long) handle);
}

int rsmgr_describe(rsrc_handle_t handle, rsrc_info_t *info)
{
    return (int) syscall2(SYSCALL_RSRC_DESCRIBE, (long) handle, (long) info);
}

int rsmgr_read(rsrc_handle_t handle, void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_RSRC_READ, (long) handle, (long) buffer, (long) size);
}

int rsmgr_write(rsrc_handle_t handle, const void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_RSRC_WRITE, (long) handle, (long) buffer, (long) size);
}

int rsmgr_list(rsrc_handle_t handle, void *buffer, uint32_t size)
{
    return (int) syscall4(SYSCALL_RSRC_LIST, (long) handle, 0, (long) buffer, (long) size);
}

int rsmgr_seek(rsrc_handle_t handle, int offset, int whence)
{
    return (int) syscall3(SYSCALL_RSRC_SEEK, (long) handle, (long) offset, (long) whence);
}

long rsmgr_control(rsrc_handle_t handle, uint64_t command_id, void *buffer, uint32_t size)
{
    return syscall4(SYSCALL_RSRC_CONTROL, (long) handle, (long) command_id, (long) buffer, (long) size);
}

void *rsmgr_mmap(rsrc_handle_t handle, uint64_t offset, uint64_t length, uint64_t prot)
{
    long result
        = syscall4(SYSCALL_RSRC_MMAP, (long) handle, (long) offset, (long) length, (long) prot);
    return result < 0 ? NULL : (void *) result;
}
