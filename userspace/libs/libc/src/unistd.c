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

int spawn_task(int argc, const char **argv, const int *inherit_rds, int inherit_rd_count)
{
    return (int) syscall4(
        SYSCALL_SPAWN_TASK, (long) argc, (long) argv, (long) inherit_rds, (long) inherit_rd_count);
}
