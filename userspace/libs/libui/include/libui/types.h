/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <input_events.h>
#include <libdesktop.h>
#include <libgfx.h>
#include <libui/memory.h>

#define BUI_TEXT_INPUT_MAX 32

#ifndef STB_TEXTEDIT_CHARTYPE
#define STB_TEXTEDIT_CHARTYPE char
#endif
#ifndef STB_TEXTEDIT_POSITIONTYPE
#define STB_TEXTEDIT_POSITIONTYPE int
#endif
#include <stb_textedit.h>

typedef struct _ui_wctx ui_wctx_t;
typedef struct _ui_widget ui_widget_t;
typedef struct _ui_theme ui_theme_t;
typedef uint32_t ui_id_t;

typedef enum {
    BUI_KEY_STATE_UP = 0,
    BUI_KEY_STATE_DOWN = 1,
    BUI_KEY_STATE_HOLD = 2,
} ui_key_state_t;

typedef enum {
    BUI_EVENT_NONE = 0,
    BUI_EVENT_WINDOW_CREATED,
    BUI_EVENT_WINDOW_RESIZED,
    BUI_EVENT_WINDOW_CLOSED,
    BUI_EVENT_KEY_DOWN,
    BUI_EVENT_KEY_HOLD,
    BUI_EVENT_KEY_UP,
    BUI_EVENT_MOUSE_MOVE,
    BUI_EVENT_MOUSE_BUTTON_DOWN,
    BUI_EVENT_MOUSE_BUTTON_UP,
} ui_event_type_t;

typedef struct
{
    uint32_t r, t, l, b;
} ui_rtlb_t;

typedef enum {
    BUI_AXIS_X = 0,
    BUI_AXIS_Y = 1,
    BUI_AXIS2_COUNT = 2,
} ui_axis_t;

typedef enum {
    BUI_WIDGET_SIZE_TYPE_NONE = 0,
    BUI_WIDGET_SIZE_TYPE_FIXED,
    BUI_WIDGET_SIZE_TYPE_PERCENTAGE,
    BUI_WIDGET_SIZE_TYPE_CHILDREN_SUM,
} ui_widget_size_type_t;

typedef struct
{
    ui_widget_size_type_t type;
    uint32_t value;
    float strictness;
} ui_widget_size_t;

typedef enum {
    BUI_WINDOW_STATE_ERROR = -1,
    BUI_WINDOW_STATE_PENDING = 0,
    BUI_WINDOW_STATE_ACTIVE = 1,
} ui_window_state;



typedef struct
{
    uint32_t pos_x, pos_y;
    input_mouse_buttons_t buttons_state;
} ui_mouse_state_t;

typedef struct
{
    ui_wctx_t *first_window;
    bool running;
} ui_ctx_t;

struct _ui_theme
{
    ui_theme_t *next;
    ui_theme_t *prev;

    gfx_font_t *font;
    ui_rtlb_t inner_padding;
    ui_rtlb_t outer_padding;

    uint32_t border_thickness;
    uint32_t shadow_thickness;
    uint32_t spacing;

    gfx_color_t foreground_color;
    gfx_color_t background_color;
    gfx_color_t border_color;
    gfx_color_t shadow_color;
};

typedef enum {
    BUI_WIDGET_FLAG_CLICKABLE = 1 << 0,
    BUI_WIDGET_FLAG_WITH_CHILDREN = 1 << 1,
    BUI_WIDGET_FLAG_PADDED = 1 << 2,
    BUI_WIDGET_FLAG_SCROLLABLE = 1 << 3,
} ui_widget_flags_t;

typedef struct
{
    ui_widget_t *widget;
    ui_id_t id;
} ui_key_pair_t;

typedef enum {
    BUI_WIDGET_EVENT_NONE = 0,
    BUI_WIDGET_EVENT_HOVERED = 1 << 0,
    BUI_WIDGET_EVENT_RCLICKED = 1 << 1,
    BUI_WIDGET_EVENT_LCLICKED = 1 << 2,
    BUI_WIDGET_EVENT_MCLICKED = 1 << 3,
    BUI_WIDGET_EVENT_RRELEASED = 1 << 4,
    BUI_WIDGET_EVENT_LRELEASED = 1 << 5,
    BUI_WIDGET_EVENT_MRELEASED = 1 << 6,
} ui_widget_event_t;

struct _ui_widget
{
    ui_widget_t *parent;
    ui_widget_t *first_child;
    ui_widget_t *next_sibling;
    ui_widget_t *prev_sibling;
    const char *label;
    char *string;
    size_t string_size;
    ui_theme_t *theme;
    void (*draw)(ui_wctx_t *ctx, ui_widget_t *widget);

    ui_widget_size_t semantic_size[BUI_AXIS2_COUNT];
    uint32_t computed_size[BUI_AXIS2_COUNT];
    gfx_area_t computed_area;
    ui_id_t id;
    ui_widget_event_t last_event;
    STB_TexteditState edit;
    uint32_t scroll_x, scroll_y;

    ui_widget_flags_t flags;
    ui_axis_t layout_axis;
};

struct _ui_wctx
{
    ui_wctx_t *next;
    ui_wctx_t *prev;

    ui_arena_t arena;
    ui_arena_t persistent_arena;
    const char *title;
    gfx_context_t gfx_context;
    uint32_t create_sequence;
    bool framebuffer_ready;
    input_mouse_buttons_t previous_mouse_buttons;
    ui_id_t scrollbar_drag_widget_id;
    ui_axis_t scrollbar_drag_axis;
    uint32_t scrollbar_drag_offset;
    ui_widget_t *widget_tree;
    ui_widget_t *prev_widget_tree;
    ui_widget_t *current_widget;
    ui_theme_t *themes_head;
    ui_theme_t *themes_tail;

    ui_key_pair_t *key_pairs;
    size_t key_pairs_count;
    size_t key_pairs_capacity;

    ui_mouse_state_t mouse_state;
    ui_key_state_t keyboard_keys[128];
    desktop_event_t input_event;
    ui_event_type_t input_event_type;
    input_mouse_buttons_t last_mouse_button_changed;
    ui_widget_event_t event_widget_state;
    ui_id_t event_widget_id;
    ui_id_t focused_widget_id;
    ui_id_t active_widget_id;

    uint32_t window_id;
    uint32_t width, height;

    ui_window_state state;
    window_flags_t flags;
    bool close_requested;
    bool hot_state;
    bool needs_redraw;
};
