/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_STACK_CAPACITY 16
#define UI_DEFAULT_FONT_SIZE 20
#define UI_DEFAULT_FONT_PATH "file:/system/assets/IBMPlexSans_Condensed-Medium.ttf"

static bool _desktop_connected;
static gfx_font_t _default_font;

static bool _ui_init_default_font(void)
{
    if (_default_font.data)
        return true;

    _default_font = gfx_load_font_from_file(UI_DEFAULT_FONT_PATH, UI_DEFAULT_FONT_SIZE);
    return _default_font.data != NULL;
}

static ui_wctx_t *_ui_find_window_by_sequence(ui_ctx_t *ctx, uint32_t sequence)
{
    for (ui_wctx_t *window = ctx->first_window; window; window = window->next) {
        if (window->state == UI_WINDOW_STATE_PENDING && window->create_sequence == sequence)
            return window;
    }
    return NULL;
}

static bool _ui_desktop_window_init(ui_wctx_t *wctx)
{
    if (!_desktop_connected) {
        if (desktop_connect() < 0)
            return false;
        _desktop_connected = true;
    }

    wctx->state = UI_WINDOW_STATE_PENDING;
    wctx->framebuffer_ready = false;
    wctx->create_sequence = desktop_create_window(
        (uint16_t) wctx->width,
        (uint16_t) wctx->height,
        (window_flags_t){.resizable = wctx->flags.resizable, .fullscreen = wctx->flags.fullscreen},
        wctx->title);
    return true;
}

static void _ui_desktop_window_destroy(ui_wctx_t *wctx)
{
    if (!wctx)
        return;
    if (wctx->window_id != 0)
        desktop_destroy_window((uint16_t) wctx->window_id);
    memset(&wctx->gfx_context, 0, sizeof(wctx->gfx_context));
    wctx->framebuffer_ready = false;
}

static void _handle_window_created(
    ui_wctx_t *wctx, const desktop_event_t *event, desktop_event_t *out)
{
    if (event->data.created.status != WINDOW_CREATED_SUCCESS) {
        wctx->state = UI_WINDOW_STATE_ERROR;
        return;
    }

    wctx->window_id = event->data.created.window_id;
    wctx->width = event->data.created.width;
    wctx->height = event->data.created.height;
    wctx->state = UI_WINDOW_STATE_PENDING;
    wctx->framebuffer_ready = desktop_request_window_framebuffer(
                                  (uint16_t) wctx->window_id,
                                  (uint16_t) wctx->width,
                                  (uint16_t) wctx->height)
                              == 1;

    *out = *event;
}

static void _handle_framebuffer_ready(ui_wctx_t *wctx, const desktop_event_t *event)
{
    if (desktop_handle_framebuffer_event(event, &wctx->gfx_context) == 1) {
        wctx->framebuffer_ready = true;
        wctx->state = UI_WINDOW_STATE_ACTIVE;
        wctx->hot_state = false;
        wctx->needs_redraw = true;
    }
}

static ui_id_t _ui_mix_id(ui_id_t parent_id, uint32_t child_index)
{
    uint32_t hash = parent_id ^ 0x9E3779B9u;
    hash ^= child_index + 0x85EBCA6Bu + (hash << 6) + (hash >> 2);
    return hash == 0 ? 1 : hash;
}

static ui_wctx_t *_ui_find_window_by_id(ui_ctx_t *ctx, uint32_t window_id)
{
    ui_wctx_t *current = ctx->first_window;
    while (current) {
        if (current->window_id == window_id)
            return current;
        current = current->next;
    }
    return NULL;
}

static bool _ui_desktop_poll_events(ui_ctx_t *ctx, desktop_event_t *out)
{
    desktop_event_t event = {0};
    int result = desktop_poll_event(&event);
    if (result != 0)
        return false;

    memset(out, 0, sizeof(*out));

    ui_wctx_t *wctx = NULL;
    switch (event.type) {
    case DESKTOP_EVENT_WINDOW_CREATED:
        wctx = _ui_find_window_by_sequence(ctx, event.sequence);
        if (!wctx)
            return false;
        _handle_window_created(wctx, &event, out);
        return true;
    case DESKTOP_EVENT_FRAMEBUFFER_READY:
        wctx = _ui_find_window_by_id(ctx, event.data.framebuffer_ready.id);
        if (!wctx)
            return false;
        _handle_framebuffer_ready(wctx, &event);
        *out = event;
        return true;
    case DESKTOP_EVENT_WINDOW_RESIZED:
        wctx = _ui_find_window_by_id(ctx, event.data.resized.window_id);
        if (!wctx)
            return false;
        wctx->width = event.data.resized.new_width;
        wctx->height = event.data.resized.new_height;
        wctx->framebuffer_ready = desktop_request_window_framebuffer(
                                      (uint16_t) wctx->window_id,
                                      (uint16_t) wctx->width,
                                      (uint16_t) wctx->height)
                                  == 1;
        *out = event;
        return true;
    case DESKTOP_EVENT_WINDOW_CLOSE:
        wctx = _ui_find_window_by_id(ctx, event.data.close.window_id);
        if (wctx)
            desktop_destroy_window((uint16_t) wctx->window_id);
        return false;
    case DESKTOP_EVENT_WINDOW_CLOSED:
        wctx = _ui_find_window_by_id(ctx, event.data.closed.window_id);
        if (!wctx)
            return false;
        *out = event;
        return true;
    case DESKTOP_EVENT_WINDOW_KEYBOARD:
        *out = event;
        return true;
    case DESKTOP_EVENT_WINDOW_MOUSE:
        wctx = _ui_find_window_by_id(ctx, event.data.mouse.window_id);
        if (!wctx)
            return false;
        *out = event;
        return true;
    default:
        return false;
    }
}

bool ui_init_context(ui_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(ui_ctx_t));
    if (!_ui_init_default_font())
        return false;
    ctx->running = true;
    return true;
}

void ui_deinit_context(ui_ctx_t *ctx)
{
    ui_wctx_t *current = ctx->first_window;
    while (current) {
        ui_wctx_t *next = current->next;
        ui_destroy_window(ctx, current);
        current = next;
    }
}

ui_wctx_t *ui_new_window(ui_ctx_t *ctx, const char *title, int w, int h, window_flags_t flags)
{
    ui_wctx_t *wctx = calloc(1, sizeof(ui_wctx_t));
    if (!wctx)
        return NULL;

    wctx->title = title;
    wctx->flags = flags;
    wctx->width = w;
    wctx->height = h;
    wctx->state = UI_WINDOW_STATE_PENDING;

    if (!_ui_desktop_window_init(wctx)) {
        free(wctx);
        return NULL;
    }

    wctx->next = ctx->first_window;
    if (ctx->first_window)
        ctx->first_window->prev = wctx;
    ctx->first_window = wctx;

    ui_arena_init(&wctx->arena);
    ui_arena_init(&wctx->persistent_arena);

    return wctx;
}

void ui_destroy_window(ui_ctx_t *ctx, ui_wctx_t *wctx)
{
    if (wctx->prev)
        wctx->prev->next = wctx->next;
    if (wctx->next)
        wctx->next->prev = wctx->prev;
    if (ctx->first_window == wctx)
        ctx->first_window = wctx->next;

    _ui_desktop_window_destroy(wctx);
    ui_arena_free(&wctx->arena);
    ui_arena_free(&wctx->persistent_arena);
    free(wctx->key_pairs);
    free(wctx);
}

static bool _ui_area_contains(gfx_area_t area, uint32_t x, uint32_t y)
{
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

static void _ui_clear_widget_event_state(ui_wctx_t *wctx)
{
    wctx->event_widget_state = UI_WIDGET_EVENT_NONE;
    wctx->event_widget_id = 0;
}

static void _ui_handle_click_event(ui_wctx_t *wctx)
{
    ui_mouse_state_t mouse_state = wctx->mouse_state;
    for (size_t i = 0; i < wctx->key_pairs_count; i++) {
        ui_widget_t *widget = wctx->key_pairs[i].widget;
        if (!(widget->flags & UI_WIDGET_FLAG_CLICKABLE))
            continue;

        ui_widget_event_t event = UI_WIDGET_EVENT_HOVERED;
        if (mouse_state.buttons_state & INPUT_MOUSE_BUTTON_LEFT)
            event |= UI_WIDGET_EVENT_LCLICKED;
        if (mouse_state.buttons_state & INPUT_MOUSE_BUTTON_RIGHT)
            event |= UI_WIDGET_EVENT_RCLICKED;
        if (mouse_state.buttons_state & INPUT_MOUSE_BUTTON_MIDDLE)
            event |= UI_WIDGET_EVENT_MCLICKED;

        if (_ui_area_contains(widget->computed_area, mouse_state.pos_x, mouse_state.pos_y)) {
            widget->last_event |= event;
            if (wctx->event_widget_id != widget->id)
                wctx->event_widget_state = UI_WIDGET_EVENT_NONE;
            wctx->event_widget_state |= event;
            wctx->event_widget_id = widget->id;
            wctx->focused_widget_id = widget->id;
            wctx->active_widget_id = widget->id;
            return;
        }
    }

    wctx->focused_widget_id = 0;
    wctx->active_widget_id = 0;
    _ui_clear_widget_event_state(wctx);
}

static void _ui_handle_release_event(ui_wctx_t *wctx, input_mouse_buttons_t button)
{
    ui_mouse_state_t mouse_state = wctx->mouse_state;
    ui_widget_event_t clicked = UI_WIDGET_EVENT_NONE, released = UI_WIDGET_EVENT_NONE;

    if (button & INPUT_MOUSE_BUTTON_LEFT) {
        clicked |= UI_WIDGET_EVENT_LCLICKED;
        released |= UI_WIDGET_EVENT_LRELEASED;
    }
    if (button & INPUT_MOUSE_BUTTON_RIGHT) {
        clicked |= UI_WIDGET_EVENT_RCLICKED;
        released |= UI_WIDGET_EVENT_RRELEASED;
    }
    if (button & INPUT_MOUSE_BUTTON_MIDDLE) {
        clicked |= UI_WIDGET_EVENT_MCLICKED;
        released |= UI_WIDGET_EVENT_MRELEASED;
    }

    for (size_t i = 0; i < wctx->key_pairs_count; i++) {
        ui_widget_t *widget = wctx->key_pairs[i].widget;
        if (!(widget->flags & UI_WIDGET_FLAG_CLICKABLE))
            continue;

        if (_ui_area_contains(widget->computed_area, mouse_state.pos_x, mouse_state.pos_y)) {
            ui_widget_event_t event = UI_WIDGET_EVENT_HOVERED | released;

            widget->last_event |= event;
            if (wctx->event_widget_id != widget->id)
                wctx->event_widget_state = UI_WIDGET_EVENT_NONE;
            wctx->event_widget_state &= ~clicked;
            wctx->event_widget_state |= event;
            wctx->event_widget_id = widget->id;
            if (button & INPUT_MOUSE_BUTTON_LEFT)
                wctx->active_widget_id = 0;
            return;
        }
    }

    if (button & INPUT_MOUSE_BUTTON_LEFT)
        wctx->active_widget_id = 0;
    _ui_clear_widget_event_state(wctx);
}

static void _ui_handle_mouse_hover(ui_wctx_t *wctx)
{
    ui_mouse_state_t mouse_state = wctx->mouse_state;
    for (size_t i = 0; i < wctx->key_pairs_count; i++) {
        ui_widget_t *widget = wctx->key_pairs[i].widget;
        if (_ui_area_contains(widget->computed_area, mouse_state.pos_x, mouse_state.pos_y)) {
            if (wctx->event_widget_id == widget->id
                && (wctx->event_widget_state & UI_WIDGET_EVENT_HOVERED))
                return;

            widget->last_event |= UI_WIDGET_EVENT_HOVERED;
            if (wctx->event_widget_id != widget->id)
                wctx->event_widget_state = UI_WIDGET_EVENT_NONE;
            wctx->event_widget_state |= UI_WIDGET_EVENT_HOVERED;
            wctx->event_widget_id = widget->id;
            return;
        }
    }

    _ui_clear_widget_event_state(wctx);
}

bool ui_pump_events(ui_ctx_t *ctx)
{
    for (ui_wctx_t *current = ctx->first_window; current; current = current->next) {
        memset(&current->input_event, 0, sizeof(current->input_event));
        current->input_event_type = UI_EVENT_NONE;
    }

    desktop_event_t event = {0};
    if (!_ui_desktop_poll_events(ctx, &event)) {
        usleep(1);
        return true;
    }

    ui_wctx_t *wctx = NULL;
    switch (event.type) {
    case DESKTOP_EVENT_WINDOW_CREATED:
        wctx = _ui_find_window_by_sequence(ctx, event.sequence);
        break;
    case DESKTOP_EVENT_WINDOW_CLOSED:
        wctx = _ui_find_window_by_id(ctx, event.data.closed.window_id);
        break;
    case DESKTOP_EVENT_WINDOW_RESIZED:
        wctx = _ui_find_window_by_id(ctx, event.data.resized.window_id);
        break;
    case DESKTOP_EVENT_WINDOW_KEYBOARD:
        wctx = _ui_find_window_by_id(ctx, event.data.keyboard.window_id);
        break;
    case DESKTOP_EVENT_WINDOW_MOUSE:
        wctx = _ui_find_window_by_id(ctx, event.data.mouse.window_id);
        break;
    default:
        break;
    }
    if (wctx) {
        wctx->input_event = event;
        wctx->input_event_type = UI_EVENT_NONE;
        wctx->needs_redraw = true;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_CLOSED) {
        if (wctx) {
            wctx->close_requested = true;
            wctx->state = UI_WINDOW_STATE_PENDING;
        }
        ctx->running = false;
        return false;
    }

    if (!wctx)
        return true;

    switch (event.type) {
    case DESKTOP_EVENT_WINDOW_CREATED:
        wctx->input_event_type = UI_EVENT_WINDOW_CREATED;
        wctx->width = event.data.created.width;
        wctx->height = event.data.created.height;
        break;
    case DESKTOP_EVENT_WINDOW_RESIZED:
        wctx->input_event_type = UI_EVENT_WINDOW_RESIZED;
        wctx->width = event.data.resized.new_width;
        wctx->height = event.data.resized.new_height;
        _ui_clear_widget_event_state(wctx);
        break;
    case DESKTOP_EVENT_WINDOW_MOUSE: {
        uint8_t previous = wctx->previous_mouse_buttons;
        uint8_t current = event.data.mouse.mouse.buttons;
        input_mouse_buttons_t changed = previous ^ current;
        wctx->previous_mouse_buttons = current;
        wctx->last_mouse_button_changed = changed;

        if (changed != 0) {
            if (current & changed) {
                wctx->input_event_type = UI_EVENT_MOUSE_BUTTON_DOWN;
            } else {
                wctx->input_event_type = UI_EVENT_MOUSE_BUTTON_UP;
            }
            wctx->mouse_state.pos_x = (uint32_t) event.data.mouse.mouse.x;
            wctx->mouse_state.pos_y = (uint32_t) event.data.mouse.mouse.y;
            if (wctx->input_event_type == UI_EVENT_MOUSE_BUTTON_DOWN) {
                wctx->mouse_state.buttons_state |= changed;
                _ui_handle_click_event(wctx);
            } else {
                wctx->mouse_state.buttons_state &= (input_mouse_buttons_t) ~changed;
                if (changed & INPUT_MOUSE_BUTTON_LEFT)
                    wctx->scrollbar_drag_widget_id = 0;
                _ui_handle_release_event(wctx, changed);
            }
        } else {
            wctx->input_event_type = UI_EVENT_MOUSE_MOVE;
            wctx->mouse_state.pos_x = (uint32_t) event.data.mouse.mouse.x;
            wctx->mouse_state.pos_y = (uint32_t) event.data.mouse.mouse.y;
            _ui_handle_mouse_hover(wctx);
        }
        break;
    }
    case DESKTOP_EVENT_WINDOW_KEYBOARD: {
        uint8_t scancode = event.data.keyboard.keyboard.scancode;
        switch (event.data.keyboard.keyboard.action) {
        case INPUT_KEYBOARD_ACTION_RELEASED:
            wctx->input_event_type = UI_EVENT_KEY_UP;
            if (scancode < 128)
                wctx->keyboard_keys[scancode] = UI_KEY_STATE_UP;
            break;
        case INPUT_KEYBOARD_ACTION_HOLD:
            wctx->input_event_type = UI_EVENT_KEY_HOLD;
            if (scancode < 128)
                wctx->keyboard_keys[scancode] = UI_KEY_STATE_HOLD;
            break;
        case INPUT_KEYBOARD_ACTION_PRESSED:
        default:
            wctx->input_event_type = UI_EVENT_KEY_DOWN;
            if (scancode < 128)
                wctx->keyboard_keys[scancode] = UI_KEY_STATE_DOWN;
            break;
        }
        break;
    }
    default:
        break;
    }

    wctx->hot_state = false;
    return true;
}

static ui_widget_t *_ui_find_widget_by_id(ui_widget_t *widget, ui_id_t id)
{
    if (widget == NULL)
        return NULL;
    if (widget->id == id)
        return widget;
    for (ui_widget_t *child = widget->first_child; child; child = child->next_sibling) {
        ui_widget_t *found = _ui_find_widget_by_id(child, id);
        if (found)
            return found;
    }
    return NULL;
}

ui_widget_t *ui_push_new_child(ui_wctx_t *wctx, ui_widget_t *child)
{
    ui_widget_t *widget = ui_arena_alloc(&wctx->arena, sizeof(ui_widget_t));
    if (widget == NULL)
        return NULL;

    *widget = *child;
    ui_widget_t *parent = wctx->current_widget;
    widget->parent = parent;

    uint32_t child_index = 0;
    if (parent->first_child == NULL) {
        parent->first_child = widget;
    } else {
        ui_widget_t *sibling = parent->first_child;
        child_index = 1;
        while (sibling->next_sibling != NULL) {
            sibling = sibling->next_sibling;
            child_index++;
        }
        sibling->next_sibling = widget;
        widget->prev_sibling = sibling;
    }
    widget->id = _ui_mix_id(parent ? parent->id : 0, child_index);

    ui_widget_t *prev = _ui_find_widget_by_id(wctx->prev_widget_tree, widget->id);
    if (prev) {
        widget->scroll_x = prev->scroll_x;
        widget->scroll_y = prev->scroll_y;
    }

    return widget;
}

ui_widget_t *ui_push_new_parent(ui_wctx_t *wctx, ui_widget_t *parent)
{
    ui_widget_t *widget = ui_push_new_child(wctx, parent);
    if (widget == NULL)
        return NULL;
    widget->flags |= UI_WIDGET_FLAG_WITH_CHILDREN;
    wctx->current_widget = widget;
    return widget;
}

void ui_pop_widget(ui_wctx_t *wctx)
{
    if (wctx->current_widget == NULL || wctx->current_widget->parent == NULL)
        return;
    if (wctx->current_widget->first_child == NULL)
        wctx->current_widget->flags &= ~UI_WIDGET_FLAG_WITH_CHILDREN;
    wctx->current_widget = wctx->current_widget->parent;
}

bool ui_begin_window(ui_wctx_t *wctx)
{
    if (wctx->state != UI_WINDOW_STATE_ACTIVE || wctx->close_requested)
        return false;
    if (!wctx->needs_redraw)
        return false;

    gfx_begin_frame(&wctx->gfx_context);

    if (!wctx->hot_state) {
        wctx->prev_widget_tree = wctx->widget_tree;
        ui_arena_t prev_arena = wctx->persistent_arena;
        wctx->persistent_arena = wctx->arena;
        wctx->arena = prev_arena;

        if (wctx->arena.head == NULL)
            ui_arena_init(&wctx->arena);
        else
            ui_arena_reset(&wctx->arena);
        if (wctx->arena.head == NULL)
            return false;

        wctx->key_pairs_count = 0;
        wctx->themes_head = NULL;
        wctx->themes_tail = NULL;
        ui_theme_t default_theme = (ui_theme_t){
            .font = &_default_font,
            .inner_padding = {7, 7, 7, 7},
            .foreground_color = {0xFF, 0xB3, 0xB3, 0xB3},
            .background_color = {0xFF, 0x21, 0x21, 0x21},
            .border_color = {0xFF, 0x00, 0x00, 0x00},
            .shadow_color = {0x33, 0xFF, 0xFF, 0xFF},
            .border_thickness = 1,
            .shadow_thickness = 1,
            .spacing = 7,
        };
        ui_push_theme(wctx, &default_theme);

        uint32_t pad_h = default_theme.inner_padding.l + default_theme.inner_padding.r;
        uint32_t pad_v = default_theme.inner_padding.t + default_theme.inner_padding.b;
        uint32_t content_width = wctx->width > pad_h ? wctx->width - pad_h : 0;
        uint32_t content_height = wctx->height > pad_v ? wctx->height - pad_v : 0;

        wctx->widget_tree = ui_arena_alloc(&wctx->arena, sizeof(*wctx->widget_tree));
        if (wctx->widget_tree == NULL)
            return false;

        *wctx->widget_tree = (ui_widget_t){
            .id = ui_hash_string("bui.root"),
            .semantic_size = {
                [UI_AXIS_X] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .value = content_width,
                    .strictness = 1.0f,
                },
                [UI_AXIS_Y] = {
                    .type = UI_WIDGET_SIZE_TYPE_FIXED,
                    .value = content_height,
                    .strictness = 1.0f,
                },
            },
            .computed_size = {
                [UI_AXIS_X] = content_width,
                [UI_AXIS_Y] = content_height,
            },
            .computed_area = {
                .x = default_theme.inner_padding.l,
                .y = default_theme.inner_padding.t,
                .width = content_width,
                .height = content_height,
                },
            .flags = UI_WIDGET_FLAG_WITH_CHILDREN,
            .layout_axis = UI_AXIS_Y,
        };
        wctx->current_widget = wctx->widget_tree;
    }

    return true;
}

bool ui_end_window(ui_wctx_t *wctx)
{
    if (!wctx->hot_state)
        ui_autolayout(wctx);

    gfx_draw_filled_rect(
        &wctx->gfx_context,
        (gfx_rect_t){.x = 0, .y = 0, .width = wctx->width, .height = wctx->height},
        wctx->themes_head->background_color);

    for (ui_widget_t *child = wctx->widget_tree->first_child; child != NULL;
         child = child->next_sibling) {
        if (child->draw)
            child->draw(wctx, child);
    }

    gfx_end_frame(&wctx->gfx_context);
    desktop_present_window((uint16_t) wctx->window_id);
    wctx->needs_redraw = wctx->input_event_type != UI_EVENT_NONE;
    return true;
}

bool ui_is_mouse_in_area(ui_wctx_t *wctx, gfx_area_t area)
{
    return wctx->mouse_state.pos_x >= area.x && wctx->mouse_state.pos_x < area.x + area.width
           && wctx->mouse_state.pos_y >= area.y && wctx->mouse_state.pos_y < area.y + area.height;
}

bool ui_is_widget_hovered(ui_wctx_t *wctx, ui_widget_t *widget)
{
    return wctx != NULL && widget != NULL && wctx->event_widget_id == widget->id
           && (wctx->event_widget_state & UI_WIDGET_EVENT_HOVERED);
}

gfx_pos_t ui_get_mouse_pos(ui_wctx_t *wctx)
{
    return (gfx_pos_t){wctx->mouse_state.pos_x, wctx->mouse_state.pos_y};
}

bool ui_is_mouse_button_down(ui_wctx_t *wctx, input_mouse_buttons_t button)
{
    return (wctx->mouse_state.buttons_state & button) != 0;
}

bool ui_is_key_down(ui_wctx_t *wctx, input_keyboard_scancode_t keycode)
{
    return wctx && keycode < 128
           && (wctx->keyboard_keys[keycode] == UI_KEY_STATE_DOWN
               || wctx->keyboard_keys[keycode] == UI_KEY_STATE_HOLD);
}

bool ui_is_key_up(ui_wctx_t *wctx, input_keyboard_scancode_t keycode)
{
    return !wctx || keycode >= 128 || wctx->keyboard_keys[keycode] == UI_KEY_STATE_UP;
}

char ui_char_from_key_scancode(ui_wctx_t *wctx, input_keyboard_scancode_t key)
{
    if (ui_is_key_down(wctx, INPUT_KEY_LCTRL) || ui_is_key_down(wctx, INPUT_KEY_LALT))
        return '\0';

    bool shifted = ui_is_key_down(wctx, INPUT_KEY_LSHIFT) || ui_is_key_down(wctx, INPUT_KEY_RSHIFT);
    static const char normal[128] = {
        [INPUT_KEY_A] = 'a',         [INPUT_KEY_B] = 'b',           [INPUT_KEY_C] = 'c',
        [INPUT_KEY_D] = 'd',         [INPUT_KEY_E] = 'e',           [INPUT_KEY_F] = 'f',
        [INPUT_KEY_G] = 'g',         [INPUT_KEY_H] = 'h',           [INPUT_KEY_I] = 'i',
        [INPUT_KEY_J] = 'j',         [INPUT_KEY_K] = 'k',           [INPUT_KEY_L] = 'l',
        [INPUT_KEY_M] = 'm',         [INPUT_KEY_N] = 'n',           [INPUT_KEY_O] = 'o',
        [INPUT_KEY_P] = 'p',         [INPUT_KEY_Q] = 'q',           [INPUT_KEY_R] = 'r',
        [INPUT_KEY_S] = 's',         [INPUT_KEY_T] = 't',           [INPUT_KEY_U] = 'u',
        [INPUT_KEY_V] = 'v',         [INPUT_KEY_W] = 'w',           [INPUT_KEY_X] = 'x',
        [INPUT_KEY_Y] = 'y',         [INPUT_KEY_Z] = 'z',           [INPUT_KEY_1] = '1',
        [INPUT_KEY_2] = '2',         [INPUT_KEY_3] = '3',           [INPUT_KEY_4] = '4',
        [INPUT_KEY_5] = '5',         [INPUT_KEY_6] = '6',           [INPUT_KEY_7] = '7',
        [INPUT_KEY_8] = '8',         [INPUT_KEY_9] = '9',           [INPUT_KEY_0] = '0',
        [INPUT_KEY_SPACE] = ' ',     [INPUT_KEY_MINUS] = '-',       [INPUT_KEY_EQUALS] = '=',
        [INPUT_KEY_LBRACKET] = '[',  [INPUT_KEY_RBRACKET] = ']',    [INPUT_KEY_BACKSLASH] = '\\',
        [INPUT_KEY_SEMICOLON] = ';', [INPUT_KEY_APOSTROPHE] = '\'', [INPUT_KEY_BACKTICK] = '`',
        [INPUT_KEY_COMMA] = ',',     [INPUT_KEY_PERIOD] = '.',      [INPUT_KEY_SLASH] = '/',
    };
    static const char shifted_chars[128] = {
        [INPUT_KEY_A] = 'A',         [INPUT_KEY_B] = 'B',          [INPUT_KEY_C] = 'C',
        [INPUT_KEY_D] = 'D',         [INPUT_KEY_E] = 'E',          [INPUT_KEY_F] = 'F',
        [INPUT_KEY_G] = 'G',         [INPUT_KEY_H] = 'H',          [INPUT_KEY_I] = 'I',
        [INPUT_KEY_J] = 'J',         [INPUT_KEY_K] = 'K',          [INPUT_KEY_L] = 'L',
        [INPUT_KEY_M] = 'M',         [INPUT_KEY_N] = 'N',          [INPUT_KEY_O] = 'O',
        [INPUT_KEY_P] = 'P',         [INPUT_KEY_Q] = 'Q',          [INPUT_KEY_R] = 'R',
        [INPUT_KEY_S] = 'S',         [INPUT_KEY_T] = 'T',          [INPUT_KEY_U] = 'U',
        [INPUT_KEY_V] = 'V',         [INPUT_KEY_W] = 'W',          [INPUT_KEY_X] = 'X',
        [INPUT_KEY_Y] = 'Y',         [INPUT_KEY_Z] = 'Z',          [INPUT_KEY_1] = '!',
        [INPUT_KEY_2] = '@',         [INPUT_KEY_3] = '#',          [INPUT_KEY_4] = '$',
        [INPUT_KEY_5] = '%',         [INPUT_KEY_6] = '^',          [INPUT_KEY_7] = '&',
        [INPUT_KEY_8] = '*',         [INPUT_KEY_9] = '(',          [INPUT_KEY_0] = ')',
        [INPUT_KEY_SPACE] = ' ',     [INPUT_KEY_MINUS] = '_',      [INPUT_KEY_EQUALS] = '+',
        [INPUT_KEY_LBRACKET] = '{',  [INPUT_KEY_RBRACKET] = '}',   [INPUT_KEY_BACKSLASH] = '|',
        [INPUT_KEY_SEMICOLON] = ':', [INPUT_KEY_APOSTROPHE] = '"', [INPUT_KEY_BACKTICK] = '~',
        [INPUT_KEY_COMMA] = '<',     [INPUT_KEY_PERIOD] = '>',     [INPUT_KEY_SLASH] = '?',
    };

    if (key >= sizeof(normal))
        return '\0';
    return shifted ? shifted_chars[key] : normal[key];
}

void ui_push_theme(ui_wctx_t *ctx, ui_theme_t *theme)
{
    ui_theme_t *new = ui_arena_alloc(&ctx->arena, sizeof(ui_theme_t));
    memcpy(new, theme, sizeof(ui_theme_t));
    new->next = NULL;
    new->prev = ctx->themes_tail;
    if (ctx->themes_tail != NULL)
        ctx->themes_tail->next = new;
    ctx->themes_tail = new;
    if (ctx->themes_head == NULL)
        ctx->themes_head = new;
}

ui_theme_t *ui_pop_theme(ui_wctx_t *ctx)
{
    ui_theme_t *theme = ctx->themes_tail;
    if (theme != NULL) {
        ctx->themes_tail = theme->prev;
        if (theme == ctx->themes_head)
            ctx->themes_head = NULL;
        return theme;
    }
    return NULL;
}

ui_theme_t *ui_get_current_theme(ui_wctx_t *ctx)
{
    return ctx->themes_tail;
}

/*
 * Source: https://github.com/ocornut/imgui/blob/master/imgui.cpp#L2413
 */
static const uint32_t _crc32_table[256] = {
    0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
    0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
    0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
    0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
    0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
    0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
    0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
    0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
    0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
    0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
    0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
    0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
    0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
    0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
    0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
    0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
    0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C, 0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
    0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043, 0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
    0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3, 0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
    0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C, 0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
    0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652, 0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
    0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D, 0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
    0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D, 0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
    0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2, 0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
    0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530, 0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
    0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF, 0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
    0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F, 0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
    0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
    0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE, 0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
    0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321, 0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
    0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81, 0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
    0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E, 0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351,
};

ui_id_t ui_hash_string(const char *str)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; str[i] != '\0'; i++) {
        uint8_t index = (crc ^ (uint8_t) str[i]) & 0xFF;
        crc = (crc >> 8) ^ _crc32_table[index];
    }
    return ~crc;
}

void ui_push_id(ui_wctx_t *wctx, ui_id_t id, ui_widget_t *widget)
{
    if (wctx->key_pairs_capacity == 0) {
        wctx->key_pairs_capacity = 16;
        wctx->key_pairs = malloc(wctx->key_pairs_capacity * sizeof(ui_key_pair_t));
    } else if (wctx->key_pairs_count == wctx->key_pairs_capacity) {
        wctx->key_pairs_capacity *= 2;
        wctx->key_pairs = realloc(wctx->key_pairs, wctx->key_pairs_capacity * sizeof(ui_key_pair_t));
    }
    widget->id = id;
    ui_widget_t *prev = _ui_find_widget_by_id(wctx->prev_widget_tree, id);
    if (prev) {
        widget->edit = prev->edit;
        widget->scroll_x = prev->scroll_x;
        widget->scroll_y = prev->scroll_y;
    }
    wctx->key_pairs[wctx->key_pairs_count++] = (ui_key_pair_t){widget, id};
}

ui_widget_t *ui_get_by_id(ui_wctx_t *wctx, ui_id_t id)
{
    for (size_t i = 0; i < wctx->key_pairs_count; i++) {
        if (wctx->key_pairs[i].id == id)
            return wctx->key_pairs[i].widget;
    }
    return NULL;
}
