/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libui/memory.h>
#include <libui/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUI_THEME(window, theme, body) \
    ui_push_theme(window, theme); \
    {body}; \
    ui_pop_theme(window)

#define BUI_WINDOW(window, body) \
    do { \
        if (ui_begin_window(window)) { \
            {body}; \
            ui_end_window(window); \
        } \
    } while (0)

bool ui_init_context(ui_ctx_t *ctx);
void ui_deinit_context(ui_ctx_t *ctx);
bool ui_pump_events(ui_ctx_t *ctx);
void ui_autolayout(ui_wctx_t *wctx);

bool ui_is_mouse_button_down(ui_wctx_t *wctx, input_mouse_buttons_t button);
gfx_pos_t ui_get_mouse_pos(ui_wctx_t *wctx);
bool ui_is_mouse_in_area(ui_wctx_t *wctx, gfx_area_t area);

bool ui_is_key_down(ui_wctx_t *wctx, input_keyboard_scancode_t keycode);
bool ui_is_key_up(ui_wctx_t *wctx, input_keyboard_scancode_t keycode);
char ui_char_from_key_scancode(ui_wctx_t *wctx, input_keyboard_scancode_t key);

ui_widget_t *ui_push_new_child(ui_wctx_t *wctx, ui_widget_t *);
ui_widget_t *ui_push_new_parent(ui_wctx_t *wctx, ui_widget_t *);
void ui_pop_widget(ui_wctx_t *wctx);

ui_wctx_t *ui_new_window(ui_ctx_t *, const char *title, int width, int height, window_flags_t);
void ui_destroy_window(ui_ctx_t *, ui_wctx_t *);

bool ui_begin_window(ui_wctx_t *wctx);
bool ui_end_window(ui_wctx_t *wctx);

void ui_push_theme(ui_wctx_t *ctx, ui_theme_t *theme);
ui_theme_t *ui_pop_theme(ui_wctx_t *ctx);
ui_theme_t *ui_get_current_theme(ui_wctx_t *ctx);

ui_id_t ui_hash_string(const char *str);
void ui_push_id(ui_wctx_t *wctx, ui_id_t id, ui_widget_t *widget);
ui_widget_t *ui_get_by_id(ui_wctx_t *wctx, ui_id_t id);

#ifdef __cplusplus
}
#endif
