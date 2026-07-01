/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/button.h>

static ui_widget_event_t _consume_button_event(ui_wctx_t *wctx, ui_id_t id)
{
    if (wctx->event_widget_id != id)
        return UI_WIDGET_EVENT_NONE;
    ui_widget_event_t event = wctx->event_widget_state;
    wctx->event_widget_state &= UI_WIDGET_EVENT_HOVERED;
    return event;
}

static void _draw_button(ui_wctx_t *ctx, ui_widget_t *widget)
{
    gfx_area_t text_area = gfx_get_text_area(widget->theme->font, widget->label);
    uint32_t available_height = widget->computed_area.height > text_area.height
                                    ? widget->computed_area.height - text_area.height
                                    : 0;
    uint32_t text_y = widget->computed_area.y + available_height / 2 + text_area.y;
    gfx_color_t fill_color = ui_is_widget_hovered(ctx, widget)
                                 ? (gfx_color_t){0xFF, 0x30, 0x30, 0x30}
                                 : widget->theme->background_color;
    gfx_draw_filled_rect(
        &ctx->gfx_context,
        (gfx_rect_t){
            .x = widget->computed_area.x,
            .y = widget->computed_area.y,
            .width = widget->computed_area.width,
            .height = widget->computed_area.height,
            .border_thickness = widget->theme->border_thickness,
            .border_color = widget->theme->border_color,
        },
        fill_color);
    uint32_t border = widget->theme->border_thickness;
    if (widget->computed_area.width > border * 2 && widget->computed_area.height > border * 2) {
        gfx_draw_rect(
            &ctx->gfx_context,
            (gfx_rect_t){
                .x = widget->computed_area.x + border,
                .y = widget->computed_area.y + border,
                .width = widget->computed_area.width - border * 2,
                .height = widget->computed_area.height - border * 2,
                .border_thickness = widget->theme->shadow_thickness,
                .border_color = widget->theme->shadow_color,
            });
    }
    gfx_draw_text(
        &ctx->gfx_context,
        widget->theme->font,
        (gfx_pos_t){
            widget->computed_area.x + widget->theme->inner_padding.l,
            text_y,
        },
        widget->theme->foreground_color,
        widget->label);
}

ui_widget_event_t ui_button(ui_wctx_t *wctx, const char *label)
{
    ui_id_t id = ui_hash_string(label);

    if (!wctx->hot_state) {
        ui_theme_t *theme = ui_get_current_theme(wctx);
        gfx_area_t text_area = gfx_get_text_area(theme->font, label);
        uint32_t text_height = gfx_get_text_height(theme->font, label);
        ui_widget_t widget = {
            .flags = UI_WIDGET_FLAG_CLICKABLE,
            .theme = theme,
            .label = label,
            .semantic_size = {
                [UI_AXIS_X] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .value = text_area.width + theme->inner_padding.l
                             + theme->inner_padding.r,
                    .strictness = 1.0f,
                },
                [UI_AXIS_Y] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .value = text_height + theme->inner_padding.t + theme->inner_padding.b,
                    .strictness = 1.0f,
                },
            },
            .draw = _draw_button,
        };

        ui_widget_t *button = ui_push_new_child(wctx, &widget);
        ui_push_id(wctx, id, button);
        return _consume_button_event(wctx, id);
    }

    ui_widget_t *button = ui_get_by_id(wctx, id);
    if (button == NULL)
        return false;
    return _consume_button_event(wctx, id);
}
