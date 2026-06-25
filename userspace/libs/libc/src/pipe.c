/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <string.h>
#include <unistd.h>

rsrc_handle_t pipe_create(const char *name)
{
    if (name != NULL) {
        size_t name_len = strlen(name);
        if (name_len == 0 || name_len >= RSRC_NAME_MAX_LEN)
            return RSRC_INVALID_HANDLE;
    }

    long result = syscall1(SYSCALL_PIPE_CREATE, (long) name);
    return result < 0 ? RSRC_INVALID_HANDLE : (rsrc_handle_t) result;
}
