/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/text.h>

#include <string.h>

static uint32_t _line_width(gfx_font_t *font, const char *start, size_t length)
{
    char buffer[length + 1];
    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return gfx_get_text_width(font, buffer);
}

static uint32_t _line_x(ui_widget_t *widget, uint32_t line_width)
{
    if ((widget->text_flags & UI_TEXT_RIGHT) && widget->computed_area.width > line_width)
        return widget->computed_area.x + widget->computed_area.width - line_width;

    if ((widget->text_flags & UI_TEXT_CENTER) && widget->computed_area.width > line_width)
        return widget->computed_area.x + (widget->computed_area.width - line_width) / 2;

    return widget->computed_area.x;
}

static void _draw_line(
    ui_wctx_t *ctx, ui_widget_t *widget, const char *start, size_t length, uint32_t y)
{
    if (length == 0)
        return;

    char *buffer = ui_arena_alloc(&ctx->arena, length + 1);
    if (buffer == NULL)
        return;

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    uint32_t width = gfx_get_text_width(widget->theme->font, buffer);
    gfx_area_t text_area = gfx_get_text_area(widget->theme->font, buffer);
    gfx_draw_text(
        &ctx->gfx_context,
        widget->theme->font,
        (gfx_pos_t){.x = _line_x(widget, width), .y = y + text_area.y},
        widget->theme->foreground_color,
        buffer);
}

static void _draw_wrapped_text(ui_wctx_t *ctx, ui_widget_t *widget)
{
    uint32_t line_height = gfx_get_text_height(widget->theme->font, "A");
    if (line_height == 0)
        return;

    uint32_t y = widget->computed_area.y;
    uint32_t max_width = widget->computed_area.width;
    const char *line_start = widget->label;
    const char *word_start = widget->label;
    const char *last_space = NULL;

    for (const char *p = widget->label; *p != '\0'; p++) {
        bool break_line = *p == '\n';
        bool at_space = *p == ' ' || *p == '\t';
        bool at_end = p[1] == '\0';
        size_t candidate_len = (size_t) (p - line_start + (at_end && !break_line ? 1 : 0));

        if (at_space)
            last_space = p;

        if (!break_line && max_width > 0
            && _line_width(widget->theme->font, line_start, candidate_len) > max_width
            && line_start < word_start) {
            const char *line_end = last_space != NULL && last_space >= line_start ? last_space
                                                                                  : word_start;
            _draw_line(ctx, widget, line_start, (size_t) (line_end - line_start), y);
            y += line_height;
            line_start = word_start;
            last_space = NULL;
        }

        if (break_line || at_end) {
            const char *line_end = break_line ? p : p + 1;
            _draw_line(ctx, widget, line_start, (size_t) (line_end - line_start), y);
            y += line_height;
            line_start = p + 1;
            word_start = line_start;
            last_space = NULL;
            continue;
        }

        if (at_space)
            word_start = p + 1;
    }
}

static void _text_draw(ui_wctx_t *ctx, ui_widget_t *widget)
{
    if (widget->text_flags & UI_TEXT_WRAP) {
        _draw_wrapped_text(ctx, widget);
        return;
    }

    uint32_t width = gfx_get_text_width(widget->theme->font, widget->label);
    gfx_area_t text_area = gfx_get_text_area(widget->theme->font, widget->label);
    gfx_draw_text(
        &ctx->gfx_context,
        widget->theme->font,
        (gfx_pos_t){.x = _line_x(widget, width), .y = widget->computed_area.y + text_area.y},
        widget->theme->foreground_color,
        widget->label);
}

void ui_text(ui_wctx_t *wctx, const char *text, ui_text_flags_t flags)
{
    if (!wctx->hot_state) {
        ui_theme_t *theme = ui_get_current_theme(wctx);
        ui_widget_t widget = {
            .flags = UI_WIDGET_FLAG_TEXT,
            .theme = theme,
            .label = text,
            .text_flags = flags,
            .semantic_size = {
                [UI_AXIS_X] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .strictness = 1.0f,
                },
                [UI_AXIS_Y] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .strictness = 1.0f,
                },
            },
            .draw = _text_draw,
        };

        ui_push_new_child(wctx, &widget);
    }
}
