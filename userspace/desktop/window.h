/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include "./menu.h"

#include <libgfx/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct window window_t;

typedef void (*window_draw_cb_t)(
    gfx_context_t *context,
    window_t *window,
    gfx_rect_t content_rect,
    void *user);

struct window {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t pos_x;
    uint32_t pos_y;
    uint32_t saved_pos_x;
    uint32_t saved_pos_y;
    uint32_t saved_width;
    uint32_t saved_height;
    bool minimized;
    bool maximized;
    bool snapped_left;
    bool snapped_right;
    char title[64];
    menubar_t *menubar;
    void *draw_user;
    window_draw_cb_t draw_content;
};

window_t *new_window(const char *title, uint32_t width, uint32_t height);

window_t *get_window(uint32_t id);

window_t *get_active_window(void);

void draw_window(gfx_context_t *context, window_t *window);

void draw_all_windows(gfx_context_t *context);

window_t *get_window_at_pos(gfx_context_t *context, uint32_t x, uint32_t y);

void update_windows_state(gfx_context_t *context);

void window_set_draw_callback(window_t *window, window_draw_cb_t draw, void *user);

void set_default_menubar(menubar_t *menubar);

menubar_t *get_active_menubar(void);

size_t window_get_minimized_count(void);

size_t window_get_minimized_titles(const char **titles, size_t max);

bool window_unminimize_at(size_t minimized_index);
