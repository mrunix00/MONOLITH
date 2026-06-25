/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/rsmgr/rsmgr.h>

bool pipe_domain_init(void);
rsrc_status_t pipe_create(const char *name, rsrc_t **out_resource);
