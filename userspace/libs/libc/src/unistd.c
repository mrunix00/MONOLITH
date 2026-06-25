/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <resource.h>
#include <unistd.h>

int open(const char *path)
{
    return rsmgr_open(path);
}

int close(int fd)
{
    return rsmgr_close(fd);
}

int read(int fd, void *buffer, uint32_t size)
{
    return rsmgr_read(fd, buffer, size);
}

int write(int fd, const void *buffer, uint32_t size)
{
    return rsmgr_write(fd, buffer, size);
}

int getdents(int fd, void *buffer, uint32_t size)
{
    return rsmgr_list(fd, buffer, size);
}

int lseek(int fd, int offset, int whence)
{
    return rsmgr_seek(fd, offset, whence);
}

rsrc_handle_t file_create(const char *path)
{
    return (rsrc_handle_t) syscall1(SYSCALL_FILE_CREATE, (long) path);
}

rsrc_handle_t task_create(int argc, const char **argv, const int *inherit_rds, int inherit_rd_count)
{
    return (rsrc_handle_t) syscall4(
        SYSCALL_TASK_CREATE, (long) argc, (long) argv, (long) inherit_rds, (long) inherit_rd_count);
}
