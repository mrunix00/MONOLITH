/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/container.h>

#include "../common.h"

#define SCROLLBAR_WIDTH 6

static uint32_t _spacing_sum(ui_widget_t *widget, uint32_t children_count)
{
    return children_count <= 1 ? 0 : widget->theme->spacing * (children_count - 1);
}

static gfx_area_t _container_viewport(ui_widget_t *widget)
{
    uint32_t horizontal_padding = widget->theme->inner_padding.l + widget->theme->inner_padding.r;
    uint32_t vertical_padding = widget->theme->inner_padding.t + widget->theme->inner_padding.b;

    return (gfx_area_t){
        .x = widget->computed_area.x + widget->theme->inner_padding.l,
        .y = widget->computed_area.y + widget->theme->inner_padding.t,
        .width = ui_sub_or_zero(widget->computed_area.width, horizontal_padding),
        .height = ui_sub_or_zero(widget->computed_area.height, vertical_padding),
    };
}

static void _translate_widget_tree(ui_widget_t *widget, int32_t dx, int32_t dy)
{
    int64_t x = (int64_t) widget->computed_area.x + dx;
    int64_t y = (int64_t) widget->computed_area.y + dy;
    widget->computed_area.x = x > 0 ? (uint32_t) x : 0;
    widget->computed_area.y = y > 0 ? (uint32_t) y : 0;

    for (ui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling)
        _translate_widget_tree(child, dx, dy);
}

static uint32_t _container_content_extent(ui_widget_t *widget, ui_axis_t axis)
{
    gfx_area_t viewport = _container_viewport(widget);
    uint32_t extent = axis == UI_AXIS_X ? viewport.width : viewport.height;
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

static uint32_t _container_max_scroll(ui_widget_t *widget, ui_axis_t axis)
{
    gfx_area_t viewport = _container_viewport(widget);
    uint32_t viewport_size = axis == UI_AXIS_X ? viewport.width : viewport.height;
    uint32_t content_extent = _container_content_extent(widget, axis);
    return ui_sub_or_zero(content_extent, viewport_size);
}

static uint32_t _container_effective_scroll(ui_widget_t *widget, ui_axis_t axis)
{
    uint32_t scroll = axis == UI_AXIS_X ? widget->scroll_x : widget->scroll_y;
    uint32_t max_scroll = _container_max_scroll(widget, axis);
    return scroll > max_scroll ? max_scroll : scroll;
}

static void _container_set_scroll(ui_widget_t *widget, uint32_t scroll_x, uint32_t scroll_y)
{
    uint32_t old_effective_scroll_x = _container_effective_scroll(widget, UI_AXIS_X);
    uint32_t old_effective_scroll_y = _container_effective_scroll(widget, UI_AXIS_Y);
    widget->scroll_x = scroll_x;
    widget->scroll_y = scroll_y;
    uint32_t effective_scroll_x = _container_effective_scroll(widget, UI_AXIS_X);
    uint32_t effective_scroll_y = _container_effective_scroll(widget, UI_AXIS_Y);

    int32_t dx = (int32_t) old_effective_scroll_x - (int32_t) effective_scroll_x;
    int32_t dy = (int32_t) old_effective_scroll_y - (int32_t) effective_scroll_y;
    if (dx == 0 && dy == 0)
        return;

    for (ui_widget_t *child = widget->first_child; child != NULL; child = child->next_sibling)
        _translate_widget_tree(child, dx, dy);
}

static bool _area_contains(gfx_area_t area, uint32_t x, uint32_t y)
{
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

static gfx_area_t _scrollbar_thumb(
    ui_widget_t *widget, ui_axis_t axis, uint32_t max_scroll, gfx_area_t viewport)
{
    if (max_scroll == 0)
        return (gfx_area_t){0};

    uint32_t viewport_size = axis == UI_AXIS_X ? viewport.width : viewport.height;
    uint32_t content_size = viewport_size + max_scroll;
    if (viewport_size == 0 || content_size == 0)
        return (gfx_area_t){0};

    uint32_t thumb_size = (uint32_t) (((uint64_t) viewport_size * viewport_size) / content_size);
    if (thumb_size < SCROLLBAR_WIDTH)
        thumb_size = SCROLLBAR_WIDTH;
    if (thumb_size > viewport_size)
        thumb_size = viewport_size;

    uint32_t scroll = _container_effective_scroll(widget, axis);
    uint32_t travel = ui_sub_or_zero(viewport_size, thumb_size);
    uint32_t thumb_pos = max_scroll == 0 ? 0
                                         : (uint32_t) (((uint64_t) scroll * travel) / max_scroll);

    if (axis == UI_AXIS_X) {
        return (gfx_area_t){
            .x = viewport.x + thumb_pos,
            .y = viewport.y + ui_sub_or_zero(viewport.height, SCROLLBAR_WIDTH),
            .width = thumb_size,
            .height = SCROLLBAR_WIDTH,
        };
    }

    return (gfx_area_t){
        .x = viewport.x + ui_sub_or_zero(viewport.width, SCROLLBAR_WIDTH),
        .y = viewport.y + thumb_pos,
        .width = SCROLLBAR_WIDTH,
        .height = thumb_size,
    };
}

static uint32_t _scroll_from_thumb_pos(
    uint32_t thumb_pos,
    uint32_t track_pos,
    uint32_t track_size,
    uint32_t thumb_size,
    uint32_t max_scroll)
{
    uint32_t travel = ui_sub_or_zero(track_size, thumb_size);
    if (travel == 0 || max_scroll == 0)
        return 0;
    uint32_t local_thumb_pos = thumb_pos > track_pos ? thumb_pos - track_pos : 0;
    if (local_thumb_pos > travel)
        local_thumb_pos = travel;
    return (uint32_t) (((uint64_t) local_thumb_pos * max_scroll) / travel);
}

static void _begin_scrollbar_drag(
    ui_wctx_t *wctx, ui_widget_t *widget, ui_axis_t axis, gfx_area_t thumb)
{
    uint32_t mouse_pos = axis == UI_AXIS_X ? wctx->mouse_state.pos_x : wctx->mouse_state.pos_y;
    uint32_t thumb_pos = axis == UI_AXIS_X ? thumb.x : thumb.y;

    wctx->active_widget_id = widget->id;
    wctx->scrollbar_drag_widget_id = widget->id;
    wctx->scrollbar_drag_axis = axis;
    wctx->scrollbar_drag_offset = mouse_pos > thumb_pos ? mouse_pos - thumb_pos : 0;
}

static bool _handle_scrollbar_drag(
    ui_wctx_t *wctx, ui_widget_t *widget, ui_axis_t axis, uint32_t max_scroll, gfx_area_t viewport)
{
    gfx_area_t thumb = _scrollbar_thumb(widget, axis, max_scroll, viewport);
    if (thumb.width == 0 || thumb.height == 0)
        return false;

    if (wctx->input_event_type == UI_EVENT_MOUSE_BUTTON_DOWN
        && (wctx->last_mouse_button_changed & INPUT_MOUSE_BUTTON_LEFT)
        && _area_contains(thumb, wctx->mouse_state.pos_x, wctx->mouse_state.pos_y)) {
        _begin_scrollbar_drag(wctx, widget, axis, thumb);
        return true;
    }

    if (wctx->input_event_type != UI_EVENT_MOUSE_MOVE
        || wctx->scrollbar_drag_widget_id != widget->id || wctx->scrollbar_drag_axis != axis
        || !ui_is_mouse_button_down(wctx, INPUT_MOUSE_BUTTON_LEFT))
        return false;

    uint32_t mouse_pos = axis == UI_AXIS_X ? wctx->mouse_state.pos_x : wctx->mouse_state.pos_y;
    uint32_t track_pos = axis == UI_AXIS_X ? viewport.x : viewport.y;
    uint32_t track_size = axis == UI_AXIS_X ? viewport.width : viewport.height;
    uint32_t thumb_size = axis == UI_AXIS_X ? thumb.width : thumb.height;
    uint32_t thumb_pos = mouse_pos > wctx->scrollbar_drag_offset
                             ? mouse_pos - wctx->scrollbar_drag_offset
                             : 0;
    uint32_t scroll
        = _scroll_from_thumb_pos(thumb_pos, track_pos, track_size, thumb_size, max_scroll);

    if (axis == UI_AXIS_X)
        _container_set_scroll(widget, scroll, widget->scroll_y);
    else
        _container_set_scroll(widget, widget->scroll_x, scroll);

    return true;
}

static void _handle_container_scroll(ui_wctx_t *wctx, ui_widget_t *widget, gfx_area_t viewport)
{
    uint32_t max_scroll_x = _container_max_scroll(widget, UI_AXIS_X);
    uint32_t max_scroll_y = _container_max_scroll(widget, UI_AXIS_Y);
    if (_handle_scrollbar_drag(wctx, widget, UI_AXIS_X, max_scroll_x, viewport))
        return;
    if (_handle_scrollbar_drag(wctx, widget, UI_AXIS_Y, max_scroll_y, viewport))
        return;
}

static void _draw_scrollbar(
    ui_wctx_t *wctx, ui_widget_t *widget, ui_axis_t axis, uint32_t max_scroll, gfx_area_t viewport)
{
    if (max_scroll == 0)
        return;

    gfx_area_t thumb = _scrollbar_thumb(widget, axis, max_scroll, viewport);
    if (thumb.width == 0 || thumb.height == 0)
        return;

    bool hovered = _area_contains(thumb, wctx->mouse_state.pos_x, wctx->mouse_state.pos_y)
                   || (wctx->scrollbar_drag_widget_id == widget->id
                       && wctx->scrollbar_drag_axis == axis);
    gfx_color_t fill_color = hovered ? (gfx_color_t){0xFF, 0x30, 0x30, 0x30}
                                     : widget->theme->foreground_color;

    gfx_draw_filled_rect(
        &wctx->gfx_context,
        (gfx_rect_t){
            .x = thumb.x,
            .y = thumb.y,
            .width = thumb.width,
            .height = thumb.height,
        },
        fill_color);
}

static void _draw_container(ui_wctx_t *wctx, ui_widget_t *widget)
{
    gfx_area_t viewport = _container_viewport(widget);
    _handle_container_scroll(wctx, widget, viewport);

    uint32_t border_thickness = widget->theme->border_thickness;
    uint32_t shadow_thickness = widget->theme->shadow_thickness;

    gfx_draw_filled_rect(
        &wctx->gfx_context,
        (gfx_rect_t){
            .x = widget->computed_area.x,
            .y = widget->computed_area.y,
            .width = widget->computed_area.width,
            .height = widget->computed_area.height,
            .border_thickness = border_thickness,
            .border_color = widget->theme->border_color,
        },
        widget->theme->background_color);
    gfx_draw_rect(
        &wctx->gfx_context,
        (gfx_rect_t){
            .x = widget->computed_area.x + border_thickness,
            .y = widget->computed_area.y + border_thickness,
            .width = ui_sub_or_zero(widget->computed_area.width, border_thickness * 2),
            .height = ui_sub_or_zero(widget->computed_area.height, border_thickness * 2),
            .border_thickness = shadow_thickness,
            .border_color = widget->theme->shadow_color,
        });

    gfx_set_clip(&wctx->gfx_context, viewport);

    ui_widget_t *child = widget->first_child;
    while (child != NULL) {
        child->draw(wctx, child);
        child = child->next_sibling;
    }

    gfx_reset_clip(&wctx->gfx_context);

    _draw_scrollbar(wctx, widget, UI_AXIS_X, _container_max_scroll(widget, UI_AXIS_X), viewport);
    _draw_scrollbar(wctx, widget, UI_AXIS_Y, _container_max_scroll(widget, UI_AXIS_Y), viewport);
}

static void _set_scroll_axis(ui_widget_t *container, ui_axis_t axis, int32_t scroll)
{
    uint32_t scroll_value = scroll < 0 ? UINT32_MAX : (uint32_t) scroll;
    if (axis == UI_AXIS_X)
        container->scroll_x = scroll_value;
    else
        container->scroll_y = scroll_value;
}

void ui_begin_container(ui_wctx_t *wctx, const char *id, uint32_t width, uint32_t height)
{
    if (!wctx->hot_state) {
        ui_widget_t widget = {
            .theme = ui_get_current_theme(wctx),
            .flags = UI_WIDGET_FLAG_PADDED | UI_WIDGET_FLAG_SCROLLABLE,
            .layout_axis = UI_AXIS_Y,
            .semantic_size = {
                [UI_AXIS_X] = ui_size_to_semantic(width, 1.0f),
                [UI_AXIS_Y] = ui_size_to_semantic(height, 1.0f),
            },
            .draw = _draw_container,
        };
        ui_widget_t *container = ui_push_new_parent(wctx, &widget);
        if (container != NULL && id != NULL) {
            ui_id_t hashed_id = ui_hash_string(id);
            ui_push_id(wctx, hashed_id, container);
        }
    }
}

void ui_end_container(ui_wctx_t *wctx)
{
    ui_pop_widget(wctx);
}

void ui_set_container_scroll(ui_wctx_t *wctx, const char *id, ui_axis_t axis, int32_t scroll)
{
    if (wctx == NULL || id == NULL)
        return;

    ui_widget_t *container = ui_get_by_id(wctx, ui_hash_string(id));
    if (container == NULL || !(container->flags & UI_WIDGET_FLAG_SCROLLABLE))
        return;

    _set_scroll_axis(container, axis, scroll);
}
