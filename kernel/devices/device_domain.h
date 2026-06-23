/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/rsmgr/rsmgr.h>
#include <stdbool.h>

bool device_domain_init(void);
rsrc_node_t *device_register(const char *path, const rsrc_ops_t *ops, void *type_state);
