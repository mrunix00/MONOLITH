/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/column.h>

#include "../common.h"

void ui_begin_column(ui_wctx_t *wctx, uint32_t width, uint32_t height)
{
    if (!wctx->hot_state) {
        ui_widget_t widget = {
            .theme = ui_get_current_theme(wctx),
            .layout_axis = UI_AXIS_Y,
            .semantic_size = {
                [UI_AXIS_X] = ui_size_to_semantic(width, 1.0f),
                [UI_AXIS_Y] = ui_size_to_semantic(height, 1.0f),
            },
            .draw = ui_draw_children,
        };
        ui_push_new_parent(wctx, &widget);
    }
}

void ui_end_column(ui_wctx_t *wctx)
{
    ui_pop_widget(wctx);
}
