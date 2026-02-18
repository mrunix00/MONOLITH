/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <sys/syscall.h>
#include <unistd.h>

int open(const char *path, int flags)
{
    return (int) syscall2(SYSCALL_FILE_OPEN, (long) path, (long) flags);
}

int close(int fd)
{
    return (int) syscall1(SYSCALL_FILE_CLOSE, (long) fd);
}

int read(int fd, void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_FILE_READ, (long) fd, (long) buffer, (long) size);
}

int write(int fd, const void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_FILE_WRITE, (long) fd, (long) buffer, (long) size);
}

int lseek(int fd, uint32_t offset, seek_mode_t mode)
{
    return (int) syscall3(SYSCALL_FILE_SEEK, (long) fd, (long) offset, (long) mode);
}

int fstat(int fd, file_stats_t *stats)
{
    return (int) syscall2(SYSCALL_FILE_GETSTATS, (long) fd, (long) stats);
}

int getdents(int fd, void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_FILE_GETDENTS, (long) fd, (long) buffer, (long) size);
}

int unlink(const char *path)
{
    return (int) syscall1(SYSCALL_FILE_REMOVE, (long) path);
}

int mkdir(const char *path, int mode)
{
    (void) mode;
    return (int) syscall2(SYSCALL_FILE_CREATE, (long) path, (long) TYPE_DIRECTORY);
}

int rmdir(const char *path)
{
    return (int) syscall1(SYSCALL_FILE_REMOVE, (long) path);
}
