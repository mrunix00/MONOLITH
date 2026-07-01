/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/types.h>

typedef enum {
    UI_SPACER_HORIZONTAL = 1 << 0,
    UI_SPACER_VERTICAL = 1 << 1,
} ui_spacer_flags_t;

void ui_spacer(ui_wctx_t *wctx, ui_spacer_flags_t flags);
