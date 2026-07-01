/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/types.h>

#define UI_ROW(window, width, height, body) \
    ui_begin_row(window, width, height); \
    {body}; \
    ui_end_row(window)

void ui_begin_row(ui_wctx_t *wctx, uint32_t width, uint32_t height);
void ui_end_row(ui_wctx_t *wctx);
