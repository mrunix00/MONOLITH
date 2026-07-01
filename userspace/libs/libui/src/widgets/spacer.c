/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/spacer.h>

static ui_widget_size_t _spacer_size(ui_spacer_flags_t flags, ui_spacer_flags_t axis_flag)
{
    if (flags & axis_flag) {
        return (ui_widget_size_t){
            .type = UI_WIDGET_SIZE_TYPE_PERCENTAGE,
            .value = 1,
            .strictness = 0.0f,
        };
    }

    return (ui_widget_size_t){
        .type = UI_WIDGET_SIZE_TYPE_FIXED,
        .value = 0,
        .strictness = 1.0f,
    };
}

void ui_spacer(ui_wctx_t *wctx, ui_spacer_flags_t flags)
{
    if (!wctx->hot_state) {
        ui_widget_t widget = {
            .theme = ui_get_current_theme(wctx),
            .semantic_size = {
                [UI_AXIS_X] = _spacer_size(flags, UI_SPACER_HORIZONTAL),
                [UI_AXIS_Y] = _spacer_size(flags, UI_SPACER_VERTICAL),
            },
        };
        ui_push_new_child(wctx, &widget);
    }
}
