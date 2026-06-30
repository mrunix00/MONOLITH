/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/types.h>

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
    if (!(widget->flags & BUI_WIDGET_FLAG_PADDED))
        return 0;
    return axis == BUI_AXIS_X ? widget->theme->inner_padding.l : widget->theme->inner_padding.t;
}

static uint32_t _padding_after(ui_widget_t *widget, ui_axis_t axis)
{
    if (!(widget->flags & BUI_WIDGET_FLAG_PADDED))
        return 0;
    return axis == BUI_AXIS_X ? widget->theme->inner_padding.r : widget->theme->inner_padding.b;
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
    uint32_t origin = axis == BUI_AXIS_X ? widget->computed_area.x : widget->computed_area.y;
    return origin + _padding_before(widget, axis);
}

static uint32_t _scrolled_origin(ui_widget_t *widget, ui_axis_t axis)
{
    uint32_t origin = _content_origin(widget, axis);
    uint32_t scroll = axis == BUI_AXIS_X ? widget->scroll_x : widget->scroll_y;
    return origin > scroll ? origin - scroll : 0;
}

static void _solve_standalone_sizes(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        if (w->semantic_size[axis].type != BUI_WIDGET_SIZE_TYPE_FIXED)
            continue;
        w->computed_size[axis] = w->semantic_size[axis].value;
    }
}

static void _solve_upwards_dependent_sizes(ui_wctx_t *wctx, ui_axis_t axis)
{
    for (ui_widget_t *w = wctx->widget_tree; w != NULL; w = _depth_first_next(w)) {
        if (w->semantic_size[axis].type != BUI_WIDGET_SIZE_TYPE_PERCENTAGE)
            continue;

        ui_widget_t *parent = w->parent;
        if (parent != NULL && parent->layout_axis == axis) {
            uint32_t children_count = 0;
            uint32_t fixed_size = 0;
            uint32_t total_percentage = 0;
            for (ui_widget_t *c = parent->first_child; c != NULL; c = c->next_sibling) {
                children_count++;
                if (c->semantic_size[axis].type == BUI_WIDGET_SIZE_TYPE_PERCENTAGE)
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

    if (w->semantic_size[axis].type != BUI_WIDGET_SIZE_TYPE_CHILDREN_SUM)
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
        if (w->flags & BUI_WIDGET_FLAG_WITH_CHILDREN && w->layout_axis != axis)
            continue;
        uint32_t children_sum = 0, children_count = 0;
        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
            children_sum += c->computed_size[axis];
            children_count++;
        }
        children_sum += _spacing_sum(w, children_count);

        if ((w->flags & BUI_WIDGET_FLAG_SCROLLABLE) && w->layout_axis == axis)
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
        uint32_t cursor = (w->flags & BUI_WIDGET_FLAG_SCROLLABLE) ? _scrolled_origin(w, axis)
                                                                  : _content_origin(w, axis);
        for (ui_widget_t *c = w->first_child; c != NULL; c = c->next_sibling) {
            if (axis == BUI_AXIS_X) {
                c->computed_area.width = c->computed_size[axis];
                c->computed_area.x = cursor;
            } else {
                c->computed_area.height = c->computed_size[axis];
                c->computed_area.y = cursor;
            }

            if (w->layout_axis == axis)
                cursor += c->computed_size[axis]
                          + (c->next_sibling == NULL ? 0 : w->theme->spacing);
        }
    }
}

void ui_autolayout(ui_wctx_t *wctx)
{
    _solve_standalone_sizes(wctx, BUI_AXIS_X);
    _solve_standalone_sizes(wctx, BUI_AXIS_Y);

    _solve_downwards_dependent_sizes(wctx, BUI_AXIS_X);
    _solve_downwards_dependent_sizes(wctx, BUI_AXIS_Y);

    _solve_upwards_dependent_sizes(wctx, BUI_AXIS_X);
    _solve_upwards_dependent_sizes(wctx, BUI_AXIS_Y);

    _solve_downwards_dependent_sizes(wctx, BUI_AXIS_X);
    _solve_downwards_dependent_sizes(wctx, BUI_AXIS_Y);

    _solve_violations(wctx, BUI_AXIS_X);
    _solve_violations(wctx, BUI_AXIS_Y);

    _compute_areas(wctx, BUI_AXIS_X);
    _compute_areas(wctx, BUI_AXIS_Y);
    wctx->hot_state = true;
}
