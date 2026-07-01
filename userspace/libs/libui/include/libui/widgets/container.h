/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/types.h>
#include <stdint.h>

#define UI_CONTAINER(window, id, width, height, body) \
    ui_begin_container(window, id, width, height); \
    {body}; \
    ui_end_container(window)

void ui_begin_container(ui_wctx_t *wctx, const char *id, uint32_t width, uint32_t height);
void ui_end_container(ui_wctx_t *wctx);
void ui_set_container_scroll(ui_wctx_t *wctx, const char *id, ui_axis_t axis, int32_t scroll);
