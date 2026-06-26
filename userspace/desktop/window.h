/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include "./menu.h"

#include <libgfx/types.h>
#include <protocol.h>
#include <resource.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t pos_x;
    uint32_t pos_y;
    uint32_t saved_pos_x;
    uint32_t saved_pos_y;
    uint32_t saved_width;
    uint32_t saved_height;
    bool borderless;
    bool resizable;
    bool fullscreen;
    bool minimized;
    bool maximized;
    bool snapped_left;
    bool snapped_right;
    char title[64];
    menubar_t *menubar;
    uint64_t owner_task_id;
    rsrc_handle_t surface_handle;
    uint32_t *surface_pixels;
    uint16_t surface_width;
    uint16_t surface_height;
    size_t surface_size;
    uint32_t *presented_pixels;
    uint16_t presented_width;
    uint16_t presented_height;
    size_t presented_size;
    uint16_t notified_content_width;
    uint16_t notified_content_height;
    uint32_t pending_width;
    uint32_t pending_height;
    bool resize_in_flight;
} window_t;

window_t *new_window(const char *title, uint32_t width, uint32_t height, window_flags_t flags);

void window_set_screen_bounds(uint32_t width, uint32_t height);

window_t *get_window_by_owner(uint64_t owner_task_id, uint32_t id);

window_t *get_active_window(void);

void draw_all_windows(gfx_context_t *context);

bool update_windows_state(gfx_context_t *context);

void window_set_remote_surface(
    window_t *window, rsrc_handle_t handle, uint32_t *pixels, uint16_t width, uint16_t height, size_t size);

uint16_t window_get_content_width(const window_t *window);

uint16_t window_get_content_height(const window_t *window);

bool window_get_resize_target(const window_t *window, uint16_t *width, uint16_t *height);

bool window_commit_resize(window_t *window, uint16_t content_width, uint16_t content_height);

bool window_present_surface(window_t *window, uint16_t content_width, uint16_t content_height);

bool window_contains_content_point(const window_t *window, uint32_t x, uint32_t y);

void close_windows_by_owner(uint64_t owner_task_id);

bool close_window_by_id(uint32_t id);

size_t window_get_count(void);

window_t *window_get_at_index(size_t index);

void set_default_menubar(menubar_t *menubar);

menubar_t *get_active_menubar(void);

size_t window_get_minimized_count(void);

size_t window_get_minimized_titles(const char **titles, size_t max);

bool window_unminimize_at(size_t minimized_index);
