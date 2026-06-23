/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <shm.h>

rsrc_handle_t shm_create(size_t size)
{
    rsrc_handle_t root = rsmgr_open("shm:/");
    if (root < 0)
        return RSRC_INVALID_HANDLE;

    shm_create_private_in_t request = {
        .size = size,
    };

    long result = rsmgr_control(root, SHM_COMMAND_CREATE_PRIVATE, &request, sizeof(request));
    rsmgr_close(root);
    return result < 0 ? RSRC_INVALID_HANDLE : (rsrc_handle_t) result;
}
