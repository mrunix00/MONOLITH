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
    uint64_t bytes_read = 0;
    int result = rsmgr_read(fd, buffer, size, &bytes_read);
    return result < 0 ? result : (int) bytes_read;
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

int chcwd(const char *path)
{
    return (int) syscall1(SYSCALL_CHCWD, (long) path);
}

rsrc_handle_t file_create(const char *path)
{
    return (rsrc_handle_t) syscall1(SYSCALL_FILE_CREATE, (long) path);
}

int mkdir(const char *path, unsigned mode)
{
    (void) mode;

    int fd = (int) syscall1(SYSCALL_DIR_CREATE, (long) path);
    if (fd < 0)
        return fd;

    close(fd);
    return 0;
}

rsrc_handle_t task_create(
    int argc, const char **argv, const task_create_inherit_t *inherit, int inherit_count)
{
    return (rsrc_handle_t) syscall4(
        SYSCALL_TASK_CREATE, (long) argc, (long) argv, (long) inherit, (long) inherit_count);
}
