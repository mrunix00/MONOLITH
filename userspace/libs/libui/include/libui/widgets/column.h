/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/types.h>
#include <stdint.h>

#define BUI_COLUMN(window, width, height, body) \
    ui_begin_column(window, width, height); \
    {body}; \
    ui_end_column(window)

void ui_begin_column(ui_wctx_t *wctx, uint32_t width, uint32_t height);
void ui_end_column(ui_wctx_t *wctx);
