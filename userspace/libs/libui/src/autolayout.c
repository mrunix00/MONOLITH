/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/types.h>
#include <string.h>

#include "./common.h"

static ui_widget_t *_depth_first_next(ui_widget_t *widget)
{
    if (widget->first_child != NULL)
        return widget->first_child;

    while (widget != NULL) {
        if (widget->next_sibling != NULL)
            return widget->next_sibling;
        widget = widget->parent;
    }

    return NULL;
}

static uint32_t _padding_before(ui_widget_t *widget, ui_axis_t axis)
{
    if (!(widget->flags & UI_WIDGET_FLAG_PADDED))
        return 0;
    return axis == UI_AXIS_X ? widget->theme->inner_padding.l : widget->theme->inner_padding.t;
}

static uint32_t _padding_after(ui_widget_t *widget, ui_axis_t axis)
{
    if (!(widget->flags & UI_WIDGET_FLAG_PADDED))
        return 0;
    return axis == UI_AXIS_X ? widget->theme->inner_padding.r : widget->theme->inner_padding.b;
}

static uint32_t _padding_sum(ui_widget_t *widget, ui_axis_t axis)
{
    return _padding_before(widget, axis) + _padding_after(widget, axis);
}

static uint32_t _spacing_sum(ui_widget_t *widget, uint32_t children_count)
{
    return children_count <= 1 ? 0 : widget->theme->spacing * (children_count - 1);
}

static uint32_t _content_size(ui_widget_t *widget, ui_axis_t axis)
{
    return ui_sub_or_zero(widget->computed_size[axis], _padding_sum(widget, axis));
}

static uint32_t _content_origin(ui_widget_t *widget, ui_axis_t axis)
{
    uint32_t origin = axis == UI_AXIS_X ? widget->computed_area.x : widget->computed_area.y;
    return origin + _padding_before(widget, axis);
}

static uint32_t _scrollable_content_extent(ui_widget_t *widget, ui_axis_t axis)
{
    uint32_t extent = _content_size(widget, axis);
    uint32_t content_extent = 0;
    uint32_t children_count = 0;

    for (ui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling) {
        uint32_t child_size = child->computed_size[axis];
        if (widget->layout_axis == axis)
            content_extent += child_size;
        else if (child_size > content_extent)
            content_extent = child_size;
        children_count++;
    }

    if (widget->layout_axis == axis)
        content_extent += _spacing_sum(widget, children_count);

    return content_extent > extent ? content_extent : extent;
}

static uint32_t _scrollable_max_scroll(ui_widget_t *widget, ui_axis_t axis)
{
    return ui_sub_or_zero(_scrollable_content_extent(widget, axis), _content_size(widget, axis));
}

static uint32_t _text_slice_width(gfx_font_t *font, const char *start, size_t length)
{
    if (length == 0)
        return 0;

    char buffer[length + 1];
    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return gfx_get_text_width(font, buffer);
}

static uint32_t _wrapped_text_line_count(ui_widget_t *widget)
{
    uint32_t max_width = widget->computed_size[UI_AXIS_X];
    if (max_width == 0 || widget->label == NULL || widget->label[0] == '\0')
        return 1;

    uint32_t lines = 1;
    const char *line_start = widget->label;
    const char *word_start = widget->label;

    for (const char *p = widget->label; *p != '\0'; p++) {
        bool break_line = *p == '\n';
        bool at_space = *p == ' ' || *p == '\t';
        bool at_end = p[1] == '\0';
        size_t candidate_len = (size_t) (p - line_start + (at_end && !break_line ? 1 : 0));

        if (!break_line
            && _text_slice_width(widget->theme->font, line_start, candidate_len) > max_width
            && line_start < word_start) {
            lines++;
            line_start = word_start;
        }

        if (break_line) {
            lines++;
            line_start = p + 1;
            word_start = line_start;
            continue;
        }

        if (at_space)
            word_start = p + 1;
    }

    return lines;
}

static bool _text_fills_axis(ui_widget_t *widget, ui_axis_t axis)
{
    return axis == UI_AXIS_X ? widget->text_flags & UI_TEXT_FILL_HORIZONTAL
                             : widget->text_flags & UI_TEXT_FILL_VERTICAL;
}

static uint32_t _text_available_size(ui_widget_t *widget, ui_axis_t axis)
{
    ui_widget_t *parent = widget->parent;
    if (parent == NULL)
        return widget->computed_size[axis];

    if (parent->layout_axis != axis)
        return _content_size(parent, axis);

    uint32_t children_count = 0;
    uint32_t fill_count = 0;
    uint32_t fixed_size = 0;
    for (ui_widget_t *c = parent->first_child; c != NULL; c = c->next_sibling) {
        children_count++;
        if ((c->flags & UI_WIDGET_FLAG_TEXT) && _text_fills_axis(c, axis)) {
            fill_count++;
        } else {
            fixed_size += c->computed_size[axis];
        }
    }

    uint32_t available_size = ui_sub_or_zero(
        _content_size(parent, axis), fixed_size + _spacing_sum(parent, children_count));
    return fill_count == 0 ? 0 : available_size / fill_count;
}

static void _solve_standalone_sizes(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        if (w->semantic_size[axis].type != UI_WIDGET_SIZE_TYPE_FIXED)
            continue;

        if (w->flags & UI_WIDGET_FLAG_TEXT) {
            if (_text_fills_axis(w, axis)) {
                w->computed_size[axis] = _text_available_size(w, axis);
                continue;
            }

            if (axis == UI_AXIS_X) {
                uint32_t text_width = gfx_get_text_width(w->theme->font, w->label);
                if ((w->text_flags & UI_TEXT_WRAP) && w->parent != NULL) {
                    uint32_t content_width = _content_size(w->parent, UI_AXIS_X);
                    if (content_width > 0 && content_width < text_width)
                        text_width = content_width;
                }
                w->computed_size[axis] = text_width;
            } else {
                uint32_t text_height = gfx_get_text_height(w->theme->font, w->label);
                if (w->text_flags & UI_TEXT_WRAP) {
                    uint32_t line_height = gfx_get_text_height(w->theme->font, "A");
                    text_height = _wrapped_text_line_count(w) * line_height;
                }
                w->computed_size[axis] = text_height;
            }
        } else {
            w->computed_size[axis] = w->semantic_size[axis].value;
        }
    }
}

static void _solve_upwards_dependent_sizes(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        if (w->semantic_size[axis].type != UI_WIDGET_SIZE_TYPE_PERCENTAGE)
            continue;

        ui_widget_t *parent = w->parent;
        if (parent != NULL && parent->layout_axis == axis) {
            uint32_t children_count = 0;
            uint32_t fixed_size = 0;
            uint32_t total_percentage = 0;
            for (ui_widget_t *c = parent->first_child; c != NULL; c = c->next_sibling) {
                children_count++;
                if (c->semantic_size[axis].type == UI_WIDGET_SIZE_TYPE_PERCENTAGE)
                    total_percentage += c->semantic_size[axis].value;
                else
                    fixed_size += c->computed_size[axis];
            }

            uint32_t available_size = ui_sub_or_zero(
                _content_size(parent, axis), fixed_size + _spacing_sum(parent, children_count));
            w->computed_size[axis] = total_percentage == 0
                                         ? 0
                                         : (uint32_t) (((uint64_t) available_size
                                                        * w->semantic_size[axis].value)
                                                       / total_percentage);
        } else {
            w->computed_size[axis] = _content_size(parent, axis) * w->semantic_size[axis].value;
        }
    }
}

static void _solve_downwards_dependent_size(ui_widget_t *w, ui_axis_t axis)
{
    if (w == NULL)
        return;

    for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling)
        _solve_downwards_dependent_size(c, axis);

    if (w->semantic_size[axis].type != UI_WIDGET_SIZE_TYPE_CHILDREN_SUM)
        return;

    w->computed_size[axis] = 0;
    uint32_t children_count = 0;
    for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
        if (w->layout_axis == axis)
            w->computed_size[axis] += c->computed_size[axis];
        else if (c->computed_size[axis] > w->computed_size[axis])
            w->computed_size[axis] = c->computed_size[axis];
        children_count++;
    }
    w->computed_size[axis] += _spacing_sum(w, children_count);
    w->computed_size[axis] += _padding_sum(w, axis);
}

static void _solve_downwards_dependent_sizes(ui_wctx_t *wctx, ui_axis_t axis)
{
    _solve_downwards_dependent_size(wctx->widget_tree, axis);
}

static void _solve_violations(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        if (w->flags & UI_WIDGET_FLAG_WITH_CHILDREN && w->layout_axis != axis)
            continue;
        uint32_t children_sum = 0, children_count = 0;
        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
            children_sum += c->computed_size[axis];
            children_count++;
        }
        children_sum += _spacing_sum(w, children_count);

        if ((w->flags & UI_WIDGET_FLAG_SCROLLABLE) && w->layout_axis == axis)
            continue;

        uint32_t available_size = _content_size(w, axis);
        if (children_sum <= available_size)
            continue;
        uint32_t overflow = children_sum - available_size;

        uint32_t total_stealable = 0;
        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling)
            total_stealable += c->computed_size[axis] * (1.0f - c->semantic_size[axis].strictness);
        if (total_stealable <= 0)
            continue;

        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
            uint32_t max_steal = c->computed_size[axis]
                                 * (1.0f - c->semantic_size[axis].strictness);
            uint32_t steal = (uint32_t) (((uint64_t) max_steal * overflow) / total_stealable);
            if (steal > max_steal)
                steal = max_steal;
            c->computed_size[axis] -= steal;
        }
    }
}

static void _compute_areas(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        int64_t cursor = _content_origin(w, axis);
        if (w->flags & UI_WIDGET_FLAG_SCROLLABLE) {
            uint32_t scroll = axis == UI_AXIS_X ? w->scroll_x : w->scroll_y;
            uint32_t max_scroll = _scrollable_max_scroll(w, axis);
            if (scroll > max_scroll)
                scroll = max_scroll;
            cursor -= scroll;
        }

        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
            if (axis == UI_AXIS_X) {
                c->computed_area.width = c->computed_size[axis];
                c->computed_area.x = cursor > 0 ? (uint32_t) cursor : 0;
            } else {
                c->computed_area.height = c->computed_size[axis];
                c->computed_area.y = cursor > 0 ? (uint32_t) cursor : 0;
            }

            if (w->layout_axis == axis)
                cursor += c->computed_size[axis]
                          + (c->next_sibling == NULL ? 0 : w->theme->spacing);
        }
    }
}

void ui_autolayout(ui_wctx_t *wctx)
{
    _solve_standalone_sizes(wctx, UI_AXIS_X);
    _solve_standalone_sizes(wctx, UI_AXIS_Y);

    _solve_downwards_dependent_sizes(wctx, UI_AXIS_X);
    _solve_downwards_dependent_sizes(wctx, UI_AXIS_Y);

    _solve_upwards_dependent_sizes(wctx, UI_AXIS_X);
    _solve_upwards_dependent_sizes(wctx, UI_AXIS_Y);

    _solve_standalone_sizes(wctx, UI_AXIS_X);
    _solve_standalone_sizes(wctx, UI_AXIS_Y);

    _solve_downwards_dependent_sizes(wctx, UI_AXIS_X);
    _solve_downwards_dependent_sizes(wctx, UI_AXIS_Y);

    _solve_violations(wctx, UI_AXIS_X);
    _solve_violations(wctx, UI_AXIS_Y);

    _compute_areas(wctx, UI_AXIS_X);
    _compute_areas(wctx, UI_AXIS_Y);
    wctx->hot_state = true;
}
