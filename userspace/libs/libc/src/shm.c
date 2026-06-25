/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <shm.h>
#include <unistd.h>

rsrc_handle_t shm_create(const char *name, size_t size)
{
    long result = syscall2(SYSCALL_SHM_CREATE, (long) name, (long) size);
    return result < 0 ? RSRC_INVALID_HANDLE : (rsrc_handle_t) result;
}
