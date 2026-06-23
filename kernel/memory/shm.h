/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SHM_COMMAND_CREATE_PRIVATE = 1,
} shm_command_id_t;

typedef struct
{
    uint64_t size;
} shm_create_private_in_t;

bool shm_domain_init(void);
