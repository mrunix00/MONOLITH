/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>

void prof_start(int id);
void prof_end(int id);
void draw_perf_stats(gfx_context_t *context);
