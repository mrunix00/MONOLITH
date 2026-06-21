/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./window.h"
#include "./icons.h"
#include "./input.h"
#include "./magic.h"
#include "./protocol_server.h"

#include <libgfx.h>
#include <libgfx/fonts.h>
#include <stdlib.h>
#include <string.h>

static window_t *_window_list = NULL;
static uint32_t _window_list_capacity = 0;
static uint32_t _window_list_size = 0;
static uint32_t _next_window_id = 1;
static menubar_t *default_menubar = NULL;
static uint32_t _screen_width = 0;
static uint32_t _screen_height = 0;

extern gfx_font_t default_font;

enum {
    WINDOW_BUTTON_SIZE = 16,
    WINDOW_RESIZE_HANDLE_SIZE = 12,
    MIN_WINDOW_WIDTH = 120,
    MIN_WINDOW_HEIGHT = 80,
    WINDOW_TITLE_PADDING_X = 10,
    WINDOW_TITLE_BUTTONS_WIDTH = 60,
    WINDOW_TITLE_BUTTONS_GAP = 10,
    SNAP_ZONE_SIZE = 16,
    WINDOW_BODY_MARGIN_X = 5,
    WINDOW_BODY_BOTTOM_MARGIN = 5,
};

static const uint32_t WINDOW_BODY_TOP_OFFSET = WINDOW_TITLE_BAR_HEIGHT - BORDER_THICKNESS;

static bool _window_has_decorations(const window_t *window)
{
    if (!window)
        return false;
    return !window->borderless && !window->fullscreen;
}

static bool _point_in_rect(uint32_t x, uint32_t y, uint32_t rx, uint32_t ry, uint32_t w, uint32_t h)
{
    return x >= rx && x < rx + w && y >= ry && y < ry + h;
}

static gfx_area_t _window_content_rect_from_geom(
    const window_t *window, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!_window_has_decorations(window))
        return (gfx_area_t){.x = x, .y = y, .width = width, .height = height};
    return (gfx_area_t){
        .x = x + WINDOW_BODY_MARGIN_X,
        .y = y + WINDOW_BODY_TOP_OFFSET,
        .width = width - WINDOW_BODY_MARGIN_X * 2,
        .height = height - WINDOW_BODY_TOP_OFFSET - WINDOW_BODY_BOTTOM_MARGIN,
    };
}

static uint32_t _window_title_min_width(const char *title)
{
    uint32_t text_width = gfx_get_text_width(&default_font, title);
    uint32_t min_width = WINDOW_TITLE_PADDING_X + text_width + WINDOW_TITLE_BUTTONS_GAP
                         + WINDOW_TITLE_BUTTONS_WIDTH;
    return min_width < MIN_WINDOW_WIDTH ? MIN_WINDOW_WIDTH : min_width;
}

static uint32_t _window_min_width(const window_t *window)
{
    if (window && !_window_has_decorations(window))
        return 1;
    return _window_title_min_width(window ? window->title : NULL);
}

static void _save_window_bounds(window_t *window)
{
    window->saved_pos_x = window->pos_x;
    window->saved_pos_y = window->pos_y;
    window->saved_width = window->width;
    window->saved_height = window->height;
}

static void _restore_window_bounds(window_t *window)
{
    if (window->saved_width == 0 || window->saved_height == 0)
        return;

    window->pos_x = window->saved_pos_x;
    window->pos_y = window->saved_pos_y;
    window->width = window->saved_width;
    window->height = window->saved_height;
}

static gfx_area_t _window_geom(gfx_context_t *context, window_t *window)
{
    return window->fullscreen ? (gfx_area_t) {
        window->maximized ? 0 : window->pos_x,
        window->maximized ? TOP_BAR_HEIGHT : window->pos_y,
        window->maximized ? (uint32_t) context->width : window->width,
        window->maximized ? (uint32_t) (context->height - TOP_BAR_HEIGHT) : window->height,
    } : (gfx_area_t) {
        window->maximized ? 0 : window->pos_x,
        window->maximized ? TOP_BAR_HEIGHT : window->pos_y,
        window->maximized ? (uint32_t) context->width : window->width,
        window->maximized ? (uint32_t) (context->height - TOP_BAR_HEIGHT) : window->height,
    };
}

static void _remove_window(uint32_t id)
{
    if (id < _window_list_size) {
        protocol_release_window_surface(&_window_list[id]);
        memmove(
            &_window_list[id],
            &_window_list[id + 1],
            (_window_list_size - id - 1) * sizeof(window_t));
        _window_list_size--;
    }
}

static window_t *_window_list_add(window_t *window)
{
    if (_window_list_size == _window_list_capacity) {
        uint32_t new_capacity = _window_list_capacity * 2;
        window_t *new_list = realloc(_window_list, new_capacity * sizeof(window_t));
        if (new_list == NULL)
            return NULL;
        _window_list = new_list;
        _window_list_capacity = new_capacity;
    }
    memcpy(&_window_list[_window_list_size], window, sizeof(window_t));
    return &_window_list[_window_list_size++];
}

static void _highlight_window(uint32_t id)
{
    if (id >= _window_list_size || id == _window_list_size - 1)
        return;

    window_t window = _window_list[id];
    memmove(&_window_list[id], &_window_list[id + 1], (_window_list_size - id - 1) * sizeof(window_t));
    memcpy(&_window_list[_window_list_size - 1], &window, sizeof(window_t));
}

static int32_t _window_index_at_pos(gfx_context_t *context, uint32_t x, uint32_t y)
{
    for (int32_t i = (int32_t) _window_list_size - 1; i >= 0; i--) {
        window_t *window = &_window_list[i];
        uint32_t left = window->pos_x;
        uint32_t top = window->pos_y;
        uint32_t right = left + window->width;
        uint32_t bottom = top + window->height;

        if (window->fullscreen) {
            left = 0;
            top = 0;
            right = (uint32_t) context->width;
            bottom = (uint32_t) context->height;
        } else if (window->maximized) {
            left = 0;
            top = TOP_BAR_HEIGHT;
            right = (uint32_t) context->width;
            bottom = (uint32_t) context->height;
        }

        if (x >= left && x < right && y >= top && y < bottom)
            return i;
    }

    return -1;
}

static bool _handle_title_bar_click(
    gfx_context_t *context,
    window_t *window,
    uint32_t mouse_x,
    uint32_t mouse_y,
    window_t **dragging_window,
    uint32_t *drag_offset_x,
    uint32_t *drag_offset_y)
{
    if (!_window_has_decorations(window))
        return false;

    gfx_area_t geom = _window_geom(context, window);

    if (!_point_in_rect(mouse_x, mouse_y, geom.x, geom.y, geom.width, WINDOW_TITLE_BAR_HEIGHT)) {
        return false;
    }

    uint32_t close_button_x = geom.x + geom.width - 20;
    uint32_t maximize_button_x = geom.x + geom.width - 40;
    uint32_t minimize_button_x = window->resizable ? (geom.x + geom.width - 60)
                                                   : (geom.x + geom.width - 40);

    if (_point_in_rect(
            mouse_x, mouse_y, close_button_x, geom.y + 10, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE)) {
        protocol_send_event(
            window->owner_task_id,
            (desktop_event_t){
                .sequence = 0,
                .type = DESKTOP_EVENT_WINDOW_CLOSE,
                .data.close.window_id = window->id,
            });
        *dragging_window = NULL;
        return true;
    }
    if (window->resizable) {
        if (_point_in_rect(
                mouse_x,
                mouse_y,
                maximize_button_x,
                geom.y + 10,
                WINDOW_BUTTON_SIZE,
                WINDOW_BUTTON_SIZE)) {
            if (window->maximized || window->snapped_left || window->snapped_right) {
                window->maximized = false;
                window->snapped_left = false;
                window->snapped_right = false;
                _restore_window_bounds(window);
            } else {
                _save_window_bounds(window);
                window->maximized = true;
                window->snapped_left = false;
                window->snapped_right = false;
                window->pos_x = 0;
                window->pos_y = TOP_BAR_HEIGHT;
                window->width = (uint32_t) context->width;
                window->height = (uint32_t) (context->height - TOP_BAR_HEIGHT);
            }
            *dragging_window = NULL;
            return true;
        }
    }
    if (_point_in_rect(
            mouse_x,
            mouse_y,
            minimize_button_x,
            geom.y + 15,
            WINDOW_BUTTON_SIZE,
            WINDOW_BUTTON_SIZE)) {
        window->minimized = true;
        *dragging_window = NULL;
        return true;
    }

    *dragging_window = window;
    if (window->maximized || window->snapped_left || window->snapped_right) {
        uint32_t saved_width = window->saved_width;
        if (saved_width == 0)
            saved_width = window->width;
        *drag_offset_x = saved_width / 2;
        *drag_offset_y = mouse_y - geom.y;
    } else {
        *drag_offset_x = mouse_x - window->pos_x;
        *drag_offset_y = mouse_y - window->pos_y;
    }

    return false;
}

static bool _handle_resize_click(
    gfx_context_t *context,
    window_t *window,
    uint32_t mouse_x,
    uint32_t mouse_y,
    window_t **resizing_window,
    uint32_t *resize_offset_x,
    uint32_t *resize_offset_y)
{
    if (!window->resizable || !_window_has_decorations(window) || window->maximized
        || window->minimized) {
        return false;
    }

    gfx_area_t geom = _window_geom(context, window);
    if (!_point_in_rect(
            mouse_x,
            mouse_y,
            geom.x + geom.width - WINDOW_RESIZE_HANDLE_SIZE,
            geom.y + geom.height - WINDOW_RESIZE_HANDLE_SIZE,
            WINDOW_RESIZE_HANDLE_SIZE,
            WINDOW_RESIZE_HANDLE_SIZE)) {
        return false;
    }

    *resizing_window = window;
    *resize_offset_x = (geom.x + geom.width) - mouse_x;
    *resize_offset_y = (geom.y + geom.height) - mouse_y;
    return true;
}

window_t *new_window(const char *title, uint32_t width, uint32_t height, window_flags_t flags)
{
    if (_window_list == NULL) {
        _window_list_capacity = 10;
        _window_list = malloc(_window_list_capacity * sizeof(window_t));
        if (_window_list == NULL)
            return NULL;
    }

    window_t window = {
        .id = _next_window_id++,
        .width = width,
        .height = height,
        .pos_x = 50,
        .pos_y = 50,
        .saved_pos_x = 50,
        .saved_pos_y = 50,
        .saved_width = width,
        .saved_height = height,
        .borderless = flags.borderless,
        .resizable = flags.resizable && !flags.borderless && !flags.fullscreen,
        .fullscreen = flags.fullscreen,
        .minimized = flags.minimized,
        .maximized = flags.maximized && !flags.fullscreen,
        .snapped_left = false,
        .snapped_right = false,
        .menubar = NULL,
        .owner_task_id = 0,
        .surface_pixels = NULL,
        .surface_width = 0,
        .surface_height = 0,
        .surface_size = 0,
        .notified_content_width = 0,
        .notified_content_height = 0,
        .pending_width = 0,
        .pending_height = 0,
        .resize_in_flight = false,
    };
    size_t title_len = title ? strnlen(title, sizeof(window.title) - 1) : 0;
    memcpy((void *) window.title, title ? title : "", title_len);
    window.title[title_len] = '\0';
    uint32_t min_width = _window_min_width(&window);
    if (window.width < min_width)
        window.width = min_width;
    if (window.height < MIN_WINDOW_HEIGHT)
        window.height = MIN_WINDOW_HEIGHT;

    if (window.fullscreen) {
        window.maximized = false;
        window.pos_x = 0;
        window.pos_y = 0;
        if (_screen_width > 0)
            window.width = _screen_width;
        if (_screen_height > 0)
            window.height = _screen_height;
    }

    return _window_list_add(&window);
}

void window_set_screen_bounds(uint32_t width, uint32_t height)
{
    _screen_width = width;
    _screen_height = height;
}

window_t *get_window_by_owner(uint64_t owner_task_id, uint32_t id)
{
    for (uint32_t i = 0; i < _window_list_size; i++) {
        if (_window_list[i].id == id && _window_list[i].owner_task_id == owner_task_id)
            return &_window_list[i];
    }
    return NULL;
}

window_t *get_active_window(void)
{
    for (int32_t i = (int32_t) _window_list_size - 1; i >= 0; i--) {
        if (!_window_list[i].minimized)
            return &_window_list[i];
    }
    return NULL;
}

static void draw_window(gfx_context_t *context, window_t *window)
{
    if (window->minimized)
        return;

    gfx_area_t geom = _window_geom(context, window);
    gfx_area_t content_rect
        = _window_content_rect_from_geom(window, geom.x, geom.y, geom.width, geom.height);

    gfx_area_t prev_clip = context->clip_rect;
    gfx_set_clip(context, content_rect);
    gfx_colored_bitmap_t bitmap = {
        .width = window->surface_width,
        .height = window->surface_height,
        .data = window->surface_pixels,
    };
    gfx_draw_colored_bitmap(context, &bitmap, (gfx_pos_t){.x = content_rect.x, .y = content_rect.y});
    context->clip_rect = prev_clip;

    if (_window_has_decorations(window)) {
        uint32_t bx = geom.x + WINDOW_BODY_MARGIN_X;
        uint32_t by = geom.y + WINDOW_BODY_TOP_OFFSET;
        uint32_t bw = geom.width - WINDOW_BODY_MARGIN_X * 2;
        uint32_t bh = geom.height - WINDOW_BODY_TOP_OFFSET - WINDOW_BODY_BOTTOM_MARGIN;

        /* Top strip */
        gfx_draw_filled_rect(
            context,
            (gfx_rect_t){.x = geom.x, .y = geom.y, .width = geom.width, .height = by - geom.y},
            SURFACE_COLOR);

        /* Bottom strip */
        gfx_draw_filled_rect(
            context,
            (gfx_rect_t){
                .x = geom.x,
                .y = by + bh,
                .width = geom.width,
                .height = (geom.y + geom.height) - (by + bh),
            },
            SURFACE_COLOR);

        /* Left strip */
        gfx_draw_filled_rect(
            context,
            (gfx_rect_t){.x = geom.x, .y = by, .width = bx - geom.x, .height = bh},
            SURFACE_COLOR);

        /* Right strip */
        gfx_draw_filled_rect(
            context,
            (gfx_rect_t){
                .x = bx + bw,
                .y = by,
                .width = (geom.x + geom.width) - (bx + bw),
                .height = bh,
            },
            SURFACE_COLOR);

        /* Outer border */
        gfx_draw_rect(
            context,
            (gfx_rect_t){
                .x = geom.x,
                .y = geom.y,
                .width = geom.width,
                .height = geom.height,
                .border_color = BORDER_COLOR,
                .border_thickness = BORDER_THICKNESS,
            });

        /* Inner highlight */
        gfx_draw_rect(
            context,
            (gfx_rect_t){
                .x = geom.x + BORDER_THICKNESS,
                .y = geom.y + BORDER_THICKNESS,
                .width = geom.width - BORDER_THICKNESS * 2,
                .height = geom.height - BORDER_THICKNESS * 2,
                .border_color = BORDER_SHADOW_COLOR,
                .border_thickness = BORDER_SHADOW_THICKNESS,
            });

        gfx_draw_text(
            context, &default_font, (gfx_pos_t){geom.x + 10, geom.y + 22}, FONT_COLOR, window->title);

        uint32_t close_button_x = geom.x + geom.width - 20;
        uint32_t maximize_button_x = geom.x + geom.width - 40;
        uint32_t minimize_button_x = window->resizable ? (geom.x + geom.width - 60)
                                                       : (geom.x + geom.width - 40);

        gfx_draw_bitmap(
            context, &close_icon_bitmap, (gfx_pos_t){close_button_x, geom.y + 10}, FONT_COLOR);
        gfx_draw_bitmap(
            context, &minimize_icon_bitmap, (gfx_pos_t){minimize_button_x, geom.y + 15}, FONT_COLOR);
        if (window->resizable)
            gfx_draw_bitmap(
                context,
                &maximize_icon_bitmap,
                (gfx_pos_t){maximize_button_x, geom.y + 10},
                FONT_COLOR);

        /* Outer opaque border */
        gfx_draw_rect(
            context,
            (gfx_rect_t){
                .x = content_rect.x,
                .y = content_rect.y,
                .width = content_rect.width,
                .height = content_rect.height,
                .border_color = (gfx_color_t){.a = 0xff, .r = 0x00, .g = 0x00, .b = 0x00},
                .border_thickness = BORDER_THICKNESS,
            });
        /* Inner transparent border */
        gfx_draw_rect(
            context,
            (gfx_rect_t){
                .x = content_rect.x + BORDER_THICKNESS,
                .y = content_rect.y + BORDER_THICKNESS,
                .width = content_rect.width - BORDER_THICKNESS * 2,
                .height = content_rect.height - BORDER_THICKNESS * 2,
                .border_color = (gfx_color_t){.a = 0x33, .r = 0xff, .g = 0xff, .b = 0xff},
                .border_thickness = BORDER_THICKNESS,
            });
    }

    if (_window_has_decorations(window) && window->resizable) {
        uint32_t handle_x = geom.x + geom.width - BORDER_THICKNESS - WINDOW_RESIZE_HANDLE_SIZE;
        uint32_t handle_y = geom.y + geom.height - BORDER_THICKNESS - WINDOW_RESIZE_HANDLE_SIZE;
        for (uint32_t i = 0; i < 3; i++) {
            uint32_t offset = i * 3;
            gfx_draw_line(
                context,
                (gfx_line_t){
                    .x1 = handle_x + offset,
                    .y1 = handle_y + WINDOW_RESIZE_HANDLE_SIZE,
                    .x2 = handle_x + WINDOW_RESIZE_HANDLE_SIZE,
                    .y2 = handle_y + offset,
                    .thickness = 1,
                },
                BORDER_SHADOW_COLOR);
        }
    }
}

void draw_all_windows(gfx_context_t *context)
{
    for (uint32_t i = 0; i < _window_list_size; i++) {
        window_t *window = &_window_list[i];
        draw_window(context, window);
    }
}

bool update_windows_state(gfx_context_t *context)
{
    static window_t *dragging_window = NULL;
    static window_t *resizing_window = NULL;
    static uint32_t drag_offset_x = 0;
    static uint32_t drag_offset_y = 0;
    static uint32_t drag_start_x = 0;
    static uint32_t drag_start_y = 0;
    static uint32_t resize_offset_x = 0;
    static uint32_t resize_offset_y = 0;
    static bool prev_left = false;
    static bool drag_restore_on_move = false;
    bool changed = false;

    input_mouse_event_t mouse = get_mouse_state();
    uint32_t mouse_x = mouse.x < 0 ? 0u : (uint32_t) mouse.x;
    uint32_t mouse_y = mouse.y < 0 ? 0u : (uint32_t) mouse.y;
    bool left = (mouse.buttons & (1 << 0)) != 0;
    int32_t index = _window_index_at_pos(context, mouse_x, mouse_y);

    if (left && !prev_left && index >= 0) {
        _highlight_window((uint32_t) index);
        changed = true;
        window_t *window = &_window_list[_window_list_size - 1];
        if (_handle_resize_click(
                context,
                window,
                mouse_x,
                mouse_y,
                &resizing_window,
                &resize_offset_x,
                &resize_offset_y)) {
            dragging_window = NULL;
            prev_left = left;
            return true;
        }
        if (_handle_title_bar_click(
                context, window, mouse_x, mouse_y, &dragging_window, &drag_offset_x, &drag_offset_y)) {
            prev_left = left;
            return true;
        }
        if (dragging_window != NULL
            && (dragging_window->maximized || dragging_window->snapped_left
                || dragging_window->snapped_right)) {
            drag_restore_on_move = true;
            drag_start_x = mouse_x;
            drag_start_y = mouse_y;
        }
        if (dragging_window != NULL && !_window_has_decorations(dragging_window)) {
            dragging_window = NULL;
        }
    } else if (!left && prev_left) {
        dragging_window = NULL;
        resizing_window = NULL;
        drag_restore_on_move = false;
        changed = true;
    }

    if (dragging_window != NULL) {
        uint32_t prev_x = dragging_window->pos_x;
        uint32_t prev_y = dragging_window->pos_y;
        bool prev_maximized = dragging_window->maximized;
        bool prev_snapped_left = dragging_window->snapped_left;
        bool prev_snapped_right = dragging_window->snapped_right;
        uint32_t screen_w = (uint32_t) context->width;
        uint32_t screen_h = (uint32_t) context->height;
        uint32_t snap_top = TOP_BAR_HEIGHT + SNAP_ZONE_SIZE;
        uint32_t new_x = mouse_x > drag_offset_x ? (mouse_x - drag_offset_x) : 0;
        uint32_t new_y = mouse_y > drag_offset_y ? (mouse_y - drag_offset_y) : 0;

        if (drag_restore_on_move) {
            uint32_t dx = mouse_x > drag_start_x ? (mouse_x - drag_start_x)
                                                 : (drag_start_x - mouse_x);
            uint32_t dy = mouse_y > drag_start_y ? (mouse_y - drag_start_y)
                                                 : (drag_start_y - mouse_y);
            /* Dismiss the mouse drag operation if not within the threshold */
            if (dx < 4 && dy < 4) {
                prev_left = left;
                return changed;
            }
            dragging_window->maximized = false;
            dragging_window->snapped_left = false;
            dragging_window->snapped_right = false;
            _restore_window_bounds(dragging_window);
            dragging_window->pos_x = new_x;
            dragging_window->pos_y = new_y;
            drag_restore_on_move = false;
        } else if (dragging_window->resizable && mouse_y <= snap_top) {
            if (!dragging_window->maximized) {
                _save_window_bounds(dragging_window);
            }
            dragging_window->maximized = true;
            dragging_window->snapped_left = false;
            dragging_window->snapped_right = false;
            dragging_window->pos_x = 0;
            dragging_window->pos_y = TOP_BAR_HEIGHT;
            dragging_window->width = screen_w;
            dragging_window->height = screen_h - TOP_BAR_HEIGHT;
        } else if (dragging_window->resizable && mouse_x <= SNAP_ZONE_SIZE) {
            if (!dragging_window->snapped_left && !dragging_window->snapped_right
                && !dragging_window->maximized) {
                _save_window_bounds(dragging_window);
            }
            dragging_window->maximized = false;
            dragging_window->snapped_left = true;
            dragging_window->snapped_right = false;
            dragging_window->pos_x = 0;
            dragging_window->pos_y = TOP_BAR_HEIGHT;
            dragging_window->width = screen_w / 2;
            dragging_window->height = screen_h - TOP_BAR_HEIGHT;
        } else if (
            dragging_window->resizable && screen_w > SNAP_ZONE_SIZE
            && mouse_x >= screen_w - SNAP_ZONE_SIZE) {
            if (!dragging_window->snapped_left && !dragging_window->snapped_right
                && !dragging_window->maximized) {
                _save_window_bounds(dragging_window);
            }
            dragging_window->maximized = false;
            dragging_window->snapped_left = false;
            dragging_window->snapped_right = true;
            dragging_window->pos_x = screen_w / 2;
            dragging_window->pos_y = TOP_BAR_HEIGHT;
            dragging_window->width = screen_w / 2;
            dragging_window->height = screen_h - TOP_BAR_HEIGHT;
        } else {
            if (dragging_window->maximized || dragging_window->snapped_left
                || dragging_window->snapped_right) {
                dragging_window->maximized = false;
                dragging_window->snapped_left = false;
                dragging_window->snapped_right = false;
                _restore_window_bounds(dragging_window);
            }
            dragging_window->pos_x = new_x;
            dragging_window->pos_y = new_y;
        }

        if (dragging_window->pos_x != prev_x || dragging_window->pos_y != prev_y
            || dragging_window->maximized != prev_maximized
            || dragging_window->snapped_left != prev_snapped_left
            || dragging_window->snapped_right != prev_snapped_right) {
            changed = true;
        }
    } else if (resizing_window != NULL) {
        uint32_t prev_width
            = resizing_window->pending_width ? resizing_window->pending_width : resizing_window->width;
        uint32_t prev_height = resizing_window->pending_height ? resizing_window->pending_height
                                                               : resizing_window->height;
        uint32_t max_width = (uint32_t) context->width > resizing_window->pos_x
                                 ? (uint32_t) context->width - resizing_window->pos_x
                                 : 0;
        uint32_t max_height = (uint32_t) context->height > resizing_window->pos_y
                                  ? (uint32_t) context->height - resizing_window->pos_y
                                  : 0;
        uint64_t raw_width = (uint64_t) mouse_x + resize_offset_x;
        uint64_t raw_height = (uint64_t) mouse_y + resize_offset_y;
        uint32_t new_width = raw_width > resizing_window->pos_x
                                 ? (uint32_t) (raw_width - resizing_window->pos_x)
                                 : 0;
        uint32_t new_height = raw_height > resizing_window->pos_y
                                  ? (uint32_t) (raw_height - resizing_window->pos_y)
                                  : 0;
        uint32_t min_width = _window_min_width(resizing_window);

        if (max_width < min_width)
            max_width = min_width;
        if (max_height < MIN_WINDOW_HEIGHT)
            max_height = MIN_WINDOW_HEIGHT;
        if (new_width < min_width)
            new_width = min_width;
        if (new_height < MIN_WINDOW_HEIGHT)
            new_height = MIN_WINDOW_HEIGHT;
        if (new_width > max_width)
            new_width = max_width;
        if (new_height > max_height)
            new_height = max_height;

        resizing_window->pending_width = new_width;
        resizing_window->pending_height = new_height;

        if (new_width != prev_width || new_height != prev_height)
            changed = true;
    }

    prev_left = left;
    return changed;
}

void window_set_remote_surface(
    window_t *window, uint32_t *pixels, uint16_t width, uint16_t height, size_t size)
{
    if (!window)
        return;

    window->surface_pixels = pixels;
    window->surface_width = width;
    window->surface_height = height;
    window->surface_size = size;
}

uint16_t window_get_content_width(const window_t *window)
{
    if (!window)
        return 0;

    uint32_t width = window->width;
    if (window->fullscreen && _screen_width > 0)
        width = _screen_width;
    else if (window->maximized && _screen_width > 0)
        width = _screen_width;

    if (!_window_has_decorations(window)) {
        return width > UINT16_MAX ? UINT16_MAX : (uint16_t) width;
    }

    uint32_t total_horizontal_chrome = WINDOW_BODY_MARGIN_X * 2;
    if (width <= total_horizontal_chrome)
        return 0;
    return (uint16_t) (width - total_horizontal_chrome);
}

uint16_t window_get_content_height(const window_t *window)
{
    if (!window)
        return 0;

    uint32_t height = window->height;
    if (window->fullscreen && _screen_height > 0)
        height = _screen_height;
    else if (window->maximized && _screen_height > TOP_BAR_HEIGHT)
        height = _screen_height - TOP_BAR_HEIGHT;

    if (!_window_has_decorations(window)) {
        return height > UINT16_MAX ? UINT16_MAX : (uint16_t) height;
    }

    uint32_t total_vertical_chrome = WINDOW_BODY_TOP_OFFSET + WINDOW_BODY_BOTTOM_MARGIN;
    if (height <= total_vertical_chrome)
        return 0;
    return (uint16_t) (height - total_vertical_chrome);
}

bool window_get_resize_target(const window_t *window, uint16_t *width, uint16_t *height)
{
    if (!window || !width || !height)
        return false;

    if (window->pending_width == 0 || window->pending_height == 0) {
        *width = window_get_content_width(window);
        *height = window_get_content_height(window);
        return true;
    }

    uint32_t horizontal_chrome = _window_has_decorations(window) ? WINDOW_BODY_MARGIN_X * 2 : 0;
    uint32_t vertical_chrome = _window_has_decorations(window)
                                   ? WINDOW_BODY_TOP_OFFSET + WINDOW_BODY_BOTTOM_MARGIN
                                   : 0;
    *width = (uint16_t) (window->pending_width - horizontal_chrome);
    *height = (uint16_t) (window->pending_height - vertical_chrome);
    return true;
}

bool window_commit_resize(window_t *window, uint16_t content_width, uint16_t content_height)
{
    if (!window || !window->resize_in_flight)
        return false;
    if (content_width != window->notified_content_width
        || content_height != window->notified_content_height) {
        return false;
    }

    if (_window_has_decorations(window)) {
        window->width = (uint32_t) content_width + WINDOW_BODY_MARGIN_X * 2;
        window->height
            = (uint32_t) content_height + WINDOW_BODY_TOP_OFFSET + WINDOW_BODY_BOTTOM_MARGIN;
    } else {
        window->width = content_width;
        window->height = content_height;
    }

    window->resize_in_flight = false;
    uint16_t target_width;
    uint16_t target_height;
    if (window_get_resize_target(window, &target_width, &target_height)
        && target_width == content_width && target_height == content_height) {
        window->pending_width = 0;
        window->pending_height = 0;
    }
    return true;
}

bool window_contains_content_point(const window_t *window, uint32_t x, uint32_t y)
{
    if (!window)
        return false;

    uint32_t window_x = window->pos_x;
    uint32_t window_y = window->pos_y;
    uint32_t window_width = window->width;
    uint32_t window_height = window->height;

    if (window->fullscreen) {
        window_x = 0;
        window_y = 0;
        window_width = _screen_width;
        window_height = _screen_height;
    } else if (window->maximized) {
        window_x = 0;
        window_y = TOP_BAR_HEIGHT;
        window_width = _screen_width;
        window_height = _screen_height > TOP_BAR_HEIGHT ? _screen_height - TOP_BAR_HEIGHT : 0;
    }

    gfx_area_t content_area
        = _window_content_rect_from_geom(window, window_x, window_y, window_width, window_height);
    return _point_in_rect(
        x, y, content_area.x, content_area.y, content_area.width, content_area.height);
}

void close_windows_by_owner(uint64_t owner_task_id)
{
    if (owner_task_id == 0)
        return;

    size_t i = 0;
    while (i < _window_list_size) {
        if (_window_list[i].owner_task_id != owner_task_id) {
            i++;
            continue;
        }
        _remove_window((uint32_t) i);
    }
}

bool close_window_by_id(uint32_t id)
{
    for (uint32_t i = 0; i < _window_list_size; i++) {
        if (_window_list[i].id != id)
            continue;
        _remove_window(i);
        return true;
    }
    return false;
}

size_t window_get_count(void)
{
    return (size_t) _window_list_size;
}

window_t *window_get_at_index(size_t index)
{
    if (index >= (size_t) _window_list_size)
        return NULL;
    return &_window_list[index];
}

void set_default_menubar(menubar_t *menubar)
{
    default_menubar = menubar;
}

menubar_t *get_active_menubar(void)
{
    window_t *active = get_active_window();
    if (active && active->menubar) {
        return active->menubar;
    }
    return default_menubar;
}

size_t window_get_minimized_count(void)
{
    size_t count = 0;
    for (uint32_t i = 0; i < _window_list_size; i++) {
        if (_window_list[i].minimized)
            count++;
    }
    return count;
}

size_t window_get_minimized_titles(const char **titles, size_t max)
{
    if (!titles || max == 0)
        return 0;

    size_t count = 0;
    for (uint32_t i = 0; i < _window_list_size; i++) {
        if (!_window_list[i].minimized)
            continue;
        if (count >= max)
            break;
        titles[count++] = _window_list[i].title;
    }
    return count;
}

bool window_unminimize_at(size_t minimized_index)
{
    size_t count = 0;
    for (uint32_t i = 0; i < _window_list_size; i++) {
        if (!_window_list[i].minimized)
            continue;
        if (count == minimized_index) {
            _window_list[i].minimized = false;
            _highlight_window(i);
            return true;
        }
        count++;
    }
    return false;
}
