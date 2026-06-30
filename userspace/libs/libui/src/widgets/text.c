/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>

static void _text_draw(ui_wctx_t *ctx, ui_widget_t *widget)
{
    gfx_area_t text_area = gfx_get_text_area(widget->theme->font, widget->label);
    gfx_draw_text(
        &ctx->gfx_context,
        widget->theme->font,
        (gfx_pos_t){.x = widget->computed_area.x, .y = widget->computed_area.y + text_area.y},
        widget->theme->foreground_color,
        widget->label);
}

void ui_text(ui_wctx_t *wctx, const char *text)
{
    if (!wctx->hot_state) {
        ui_theme_t *theme = ui_get_current_theme(wctx);
        gfx_area_t text_area = gfx_get_text_area(theme->font, text);
        ui_widget_t widget = {
            .theme = theme,
            .label = text,
            .semantic_size = {
                [BUI_AXIS_X] = {
                    .type = BUI_WIDGET_SIZE_TYPE_FIXED,
                    .value = text_area.width,
                    .strictness = 1.0f,
                },
                [BUI_AXIS_Y] = {
                    .type = BUI_WIDGET_SIZE_TYPE_FIXED,
                    .value = text_area.height,
                    .strictness = 1.0f,
                },
            },
            .draw = _text_draw,
        };

        ui_push_new_child(wctx, &widget);
    }
}
