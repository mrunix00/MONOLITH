/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/libui.h>

static inline uint32_t ui_sub_or_zero(uint32_t lhs, uint32_t rhs)
{
    return lhs > rhs ? lhs - rhs : 0;
}

static inline void ui_draw_children(ui_wctx_t *wctx, ui_widget_t *widget)
{
    for (ui_widget_t *child = widget->first_child; child; child = child->next_sibling) {
        if (child->draw)
            child->draw(wctx, child);
    }
}

static inline ui_widget_size_t ui_size_to_semantic(uint32_t size, float fixed_strictness)
{
    if (size == 0) {
        return (ui_widget_size_t){
            .type = UI_WIDGET_SIZE_TYPE_CHILDREN_SUM,
            .strictness = 1.0f,
        };
    }

    if (size == UINT32_MAX) {
        return (ui_widget_size_t){
            .type = UI_WIDGET_SIZE_TYPE_PERCENTAGE,
            .value = 1.0f,
            .strictness = 0.0f,
        };
    }

    return (ui_widget_size_t){
        .type = UI_WIDGET_SIZE_TYPE_FIXED,
        .value = size,
        .strictness = fixed_strictness,
    };
}
