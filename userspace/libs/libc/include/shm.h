/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <resource.h>
#include <stddef.h>

typedef enum {
    SHM_COMMAND_CREATE_PRIVATE = 1,
} shm_command_id_t;

typedef struct
{
    uint64_t size;
} shm_create_private_in_t;

rsrc_handle_t shm_create(size_t size);
