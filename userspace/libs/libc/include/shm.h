/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <resource.h>
#include <stddef.h>

rsrc_handle_t shm_create(const char *name, size_t size);
