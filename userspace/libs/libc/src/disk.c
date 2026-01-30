/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <disk.h>
#include <sys/syscall.h>
#include <unistd.h>

FILE file_open(const char *path)
{
    return (FILE) syscall1(SYSCALL_FILE_OPEN, (long) path);
}

int file_read(FILE file, void *buffer, uint32_t size)
{
    return (int) syscall3(SYSCALL_FILE_READ, (long) file, (long) buffer, (long) size);
}

int file_seek(FILE file, uint32_t offset, seek_mode_t mode)
{
    return (int) syscall3(SYSCALL_FILE_SEEK, (long) file, (long) offset, (long) mode);
}

int file_getstats(FILE file, file_stats_t *stats) {
    return (int) syscall2(SYSCALL_FILE_GETSTATS, (long) file, (long) stats);
}
