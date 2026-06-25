/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/rsmgr/rsmgr.h>

bool shm_domain_init(void);
rsrc_status_t shm_create(const char *name, size_t size, rsrc_t **out_resource);
