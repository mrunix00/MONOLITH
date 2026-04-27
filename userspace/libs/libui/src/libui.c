/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libui/libui.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../desktop/font.h"
#include "input.h"

#define INITIAL_STACK_CAPACITY 16

#define DEFAULT_PADDING 7
#define BACKGROUND_COLOR (gfx_color_t){0xFF, 0x21, 0x21, 0x21}
#define SECONDARY_BACKGROUND_COLOR (gfx_color_t){0xFF, 0x18, 0x18, 0x18}
#define TEXT_COLOR (gfx_color_t){0xFF, 0xB3, 0xB3, 0xB3}
#define BORDER_COLOR (gfx_color_t){0xFF, 0x00, 0x00, 0x00}
#define BORDER_THICKNESS 1
#define BORDER_SHADOW (gfx_color_t){0x33, 0xFF, 0xFF, 0xFF}

static gfx_font_t _default_font = (gfx_font_t){
    .atlas = font_atlas,
    .atlas_size = (gfx_font_size_t){FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT},
    .glyphs = font_glyphs,
    .first_char = FONT_FIRST_CHAR,
    .last_char = FONT_LAST_CHAR,
    .size = (gfx_font_size_t){13, FONT_LINE_HEIGHT},
};

static inline uint32_t _sub_or_zero(uint32_t lhs, uint32_t rhs)
{
    return lhs > rhs ? lhs - rhs : 0;
}

static ui_wctx_t *_ui_find_window_by_sequence(ui_ctx_t *ctx, uint32_t sequence)
{
    ui_wctx_t *current = ctx->first_window;
    while (current) {
        if (current->create_sequence == sequence)
            return current;
        current = current->next;
    }
    return NULL;
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

static void _ui_mark_window_closed(ui_wctx_t *wctx)
{
    wctx->state = UI_WINDOW_STATE_ERROR;
    wctx->window_id = 0;
    memset(&wctx->framebuffer, 0, sizeof(wctx->framebuffer));
}

static void _ui_mark_window_closing(ui_wctx_t *wctx)
{
    wctx->state = UI_WINDOW_STATE_PENDING;
    memset(&wctx->framebuffer, 0, sizeof(wctx->framebuffer));
}

static ui_window_state _ui_framebuffer_request_state(uint16_t window_id, uint16_t w, uint16_t h)
{
    int status = desktop_request_window_framebuffer(window_id, w, h);
    if (status < 0)
        return UI_WINDOW_STATE_ERROR;
    if (status == 0)
        return UI_WINDOW_STATE_PENDING;
    return UI_WINDOW_STATE_ACTIVE;
}

bool ui_init_context(ui_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(ui_ctx_t));
    if (desktop_connect() != 0)
        return false;

    return true;
}

void ui_deinit_context(ui_ctx_t *ctx)
{
    desktop_disconnect();
    ui_wctx_t *current = ctx->first_window;
    while (current) {
        ui_wctx_t *next = current->next;
        ui_destroy_window(ctx, current);
        current = next;
    }
}

ui_wctx_t *ui_new_window(ui_ctx_t *ctx, const char *title, int w, int h, window_flags_t flags)
{
    uint32_t create_sequence = desktop_create_window(w, h, flags, title);

    ui_wctx_t *wctx = malloc(sizeof(ui_wctx_t));
    if (!wctx)
        return NULL;
    wctx->title = title;
    wctx->width = w;
    wctx->height = h;
    wctx->create_sequence = create_sequence;
    wctx->state = UI_WINDOW_STATE_PENDING;

    wctx->next = ctx->first_window;
    wctx->prev = ctx->first_window ? ctx->first_window->prev : NULL;
    if (ctx->first_window == NULL)
        ctx->first_window = wctx;

    wctx->layout_stack = malloc(INITIAL_STACK_CAPACITY * sizeof(ui_layout_t));
    if (wctx->layout_stack == NULL) {
        free(wctx);
        return NULL;
    }
    wctx->layout_count = 0;
    wctx->layout_capacity = INITIAL_STACK_CAPACITY;

    memset(wctx->keyboard_keys, INPUT_KEYACTION_UP, sizeof(wctx->keyboard_keys));
    wctx->mouse_state = (ui_mouse_state_t){0};

    return wctx;
}

void ui_destroy_window(ui_ctx_t *ctx, ui_wctx_t *wctx)
{
    wctx->prev = wctx->next;
    if (wctx->next)
        wctx->next->prev = wctx->prev;
    if (ctx->first_window == wctx)
        ctx->first_window = wctx->next;
    free(wctx->layout_stack);
    free(wctx);
}

bool ui_pump_events(ui_ctx_t *ctx)
{
    desktop_event_t event;
    if (desktop_poll_event(&event) != 0) {
        usleep(1);
        return true;
    }

    for (ui_wctx_t *wctx = ctx->first_window; wctx; wctx = wctx->next)
        wctx->last_event = (input_event_t){0};

    if (event.type == DESKTOP_EVENT_WINDOW_CREATED) {
        uint32_t sequence = event.sequence;
        ui_wctx_t *wctx = _ui_find_window_by_sequence(ctx, sequence);
        if (!wctx)
            return false;

        if (event.data.created.status != WINDOW_CREATED_SUCCESS) {
            wctx->state = UI_WINDOW_STATE_ERROR;
            return false;
        }

        wctx->window_id = event.data.created.window_id;
        wctx->width = event.data.created.width;
        wctx->height = event.data.created.height;
        wctx->state = desktop_request_window_framebuffer(wctx->window_id, wctx->width, wctx->height);
        return wctx->state != UI_WINDOW_STATE_ERROR;
    }

    if (event.type == DESKTOP_EVENT_FRAMEBUFFER_READY) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.framebuffer_ready.id);
        if (!wctx)
            return false;

        int status = desktop_handle_framebuffer_event(&event, &wctx->framebuffer);
        wctx->state = status == 1 ? UI_WINDOW_STATE_ACTIVE : UI_WINDOW_STATE_ERROR;
        return true;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_RESIZED) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.resized.window_id);
        if (!wctx)
            return false;

        wctx->width = event.data.resized.new_width;
        wctx->height = event.data.resized.new_height;
        wctx->state = _ui_framebuffer_request_state(wctx->window_id, wctx->width, wctx->height);
        return wctx->state != UI_WINDOW_STATE_ERROR;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_CLOSE) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.close.window_id);
        if (!wctx)
            return false;

        desktop_destroy_window(wctx->window_id);
        _ui_mark_window_closing(wctx);
        return false;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_CLOSED) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.closed.window_id);
        if (!wctx)
            return false;

        _ui_mark_window_closed(wctx);
        return true;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_MOUSE) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.mouse.window_id);
        if (!wctx)
            return false;

        wctx->mouse_state.pos_x = event.data.mouse.mouse.x;
        wctx->mouse_state.pos_y = event.data.mouse.mouse.y;
        wctx->mouse_state.buttons_state = event.data.mouse.mouse.buttons;

        wctx->last_event.type = INPUT_EVENT_MOUSE;
        wctx->last_event.data.mouse = event.data.mouse.mouse;
        return true;
    }

    if (event.type == DESKTOP_EVENT_WINDOW_KEYBOARD) {
        ui_wctx_t *wctx = _ui_find_window_by_id(ctx, event.data.keyboard.window_id);
        if (!wctx)
            return false;

        wctx->keyboard_keys[event.data.keyboard.keyboard.scancode]
            = event.data.keyboard.keyboard.action;
        wctx->last_event.type = INPUT_EVENT_KEYBOARD;
        wctx->last_event.data.keyboard = event.data.keyboard.keyboard;
        return true;
    }

    return true;
}

bool ui_begin_window(ui_wctx_t *wctx)
{
    if (wctx->state != UI_WINDOW_STATE_ACTIVE)
        return false;

    wctx->layout_count = 0;
    gfx_context_t *framebuffer = &wctx->framebuffer;
    gfx_begin_frame(framebuffer);
    gfx_reset_clip(framebuffer);
    gfx_draw_filled_rect(
        framebuffer,
        (gfx_rect_t){.x = 0, .y = 0, .width = framebuffer->width, .height = framebuffer->height},
        BACKGROUND_COLOR);
    ui_push_layout(
        wctx,
        (ui_layout_t){
            .type = UI_LAYOUT_VERTICAL,
            .size = (gfx_area_t){0, 0, framebuffer->width, framebuffer->height},
            .cursor_pos_x = 0,
            .cursor_pos_y = 0,
            .padding_x = 0,
            .padding_y = 0,
            .inner_margin = {0},
            .outer_margin = {0},
        });
    return true;
}

bool ui_end_window(ui_wctx_t *wctx)
{
    if (wctx->state != UI_WINDOW_STATE_ACTIVE)
        return false;
    gfx_end_frame(&wctx->framebuffer);
    return desktop_present_window((uint16_t) wctx->window_id) == 0;
}

bool ui_push_layout(ui_wctx_t *wctx, ui_layout_t layout)
{
    if (wctx->layout_count >= wctx->layout_capacity) {
        wctx->layout_capacity *= 2;
        wctx->layout_stack
            = realloc(wctx->layout_stack, wctx->layout_capacity * sizeof(ui_layout_t));
        if (wctx->layout_stack == NULL)
            return false;
    }
    wctx->layout_stack[wctx->layout_count++] = layout;
    return true;
}

ui_layout_t *ui_get_layout(ui_wctx_t *wctx)
{
    return &wctx->layout_stack[wctx->layout_count - 1];
}

void ui_advance_layout(ui_wctx_t *wctx, uint32_t w, uint32_t h)
{
    ui_layout_t *current = ui_get_layout(wctx);
    if (current->type == UI_LAYOUT_HORIZONTAL) {
        uint32_t item_right
            = _sub_or_zero(current->cursor_pos_x, current->size.x + current->inner_margin.l) + w;
        if (item_right > current->content_width)
            current->content_width = item_right;
        if (h > current->content_height)
            current->content_height = h;
        current->cursor_pos_x += w + current->padding_x;
    } else if (current->type == UI_LAYOUT_VERTICAL) {
        uint32_t item_bottom
            = _sub_or_zero(current->cursor_pos_y, current->size.y + current->inner_margin.t) + h;
        if (w > current->content_width)
            current->content_width = w;
        if (item_bottom > current->content_height)
            current->content_height = item_bottom;
        current->cursor_pos_y += h + current->padding_y;
    }
}

ui_layout_t ui_pop_layout(ui_wctx_t *wctx)
{
    ui_layout_t result = wctx->layout_stack[--wctx->layout_count];
    if (result.fit_width) {
        result.size.width = result.content_width + result.inner_margin.l + result.inner_margin.r;
    }
    if (result.fit_height) {
        result.size.height = result.content_height + result.inner_margin.t + result.inner_margin.b;
    }
    return result;
}

static uint32_t _ui_get_available_space(ui_wctx_t *wctx, uint8_t layout_type)
{
    ui_layout_t *current = ui_get_layout(wctx);
    if (layout_type == UI_LAYOUT_VERTICAL) {
        uint32_t height
            = _sub_or_zero(current->size.height, current->inner_margin.t + current->inner_margin.b);
        uint32_t used
            = _sub_or_zero(current->cursor_pos_y, current->size.y + current->inner_margin.t);
        return _sub_or_zero(height, used);
    } else if (layout_type == UI_LAYOUT_HORIZONTAL) {
        uint32_t width
            = _sub_or_zero(current->size.width, current->inner_margin.l + current->inner_margin.r);
        uint32_t used
            = _sub_or_zero(current->cursor_pos_x, current->size.x + current->inner_margin.l);
        return _sub_or_zero(width, used);
    }
    return 0;
}

void ui_begin_column(ui_wctx_t *wctx, ui_rtlb_t margin)
{
    ui_layout_t *current = ui_get_layout(wctx);
    uint32_t available_width
        = _sub_or_zero(_ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL), margin.l + margin.r);
    uint32_t available_height
        = _sub_or_zero(_ui_get_available_space(wctx, UI_LAYOUT_VERTICAL), margin.t + margin.b);

    ui_push_layout(
        wctx,
        (ui_layout_t){
            .type = UI_LAYOUT_VERTICAL,
            .size = (gfx_area_t){
                .x = current->cursor_pos_x,
                .y = current->cursor_pos_y,
                .width = available_width + margin.l + margin.r,
                .height = available_height + margin.t + margin.b,
            },
            .inner_margin = margin,
            .cursor_pos_x = current->cursor_pos_x + margin.l,
            .cursor_pos_y = current->cursor_pos_y + margin.t,
            .padding_x = 0,
            .padding_y = DEFAULT_PADDING,
            .fit_width = true,
            .fit_height = true,
        });
}

void ui_end_column(ui_wctx_t *wctx)
{
    ui_layout_t column_layout = ui_pop_layout(wctx);
    ui_advance_layout(
        wctx,
        column_layout.size.width + column_layout.outer_margin.l + column_layout.outer_margin.r,
        column_layout.size.height + column_layout.outer_margin.t + column_layout.outer_margin.b);
}

void ui_begin_row(ui_wctx_t *wctx, ui_rtlb_t margin)
{
    ui_layout_t *current = ui_get_layout(wctx);
    uint32_t available_width
        = _sub_or_zero(_ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL), margin.l + margin.r);
    uint32_t available_height
        = _sub_or_zero(_ui_get_available_space(wctx, UI_LAYOUT_VERTICAL), margin.t + margin.b);

    ui_push_layout(
        wctx,
        (ui_layout_t){
            .type = UI_LAYOUT_HORIZONTAL,
            .size = (gfx_area_t){
                .x = current->cursor_pos_x,
                .y = current->cursor_pos_y,
                .width = available_width + margin.l + margin.r,
                .height = available_height + margin.t + margin.b,
            },
            .inner_margin = margin,
            .cursor_pos_x = current->cursor_pos_x + margin.l,
            .cursor_pos_y = current->cursor_pos_y + margin.t,
            .padding_x = DEFAULT_PADDING,
            .padding_y = 0,
            .fit_width = true,
            .fit_height = true,
        });
}

void ui_end_row(ui_wctx_t *wctx)
{
    ui_layout_t row_layout = ui_pop_layout(wctx);
    ui_advance_layout(
        wctx,
        row_layout.size.width + row_layout.outer_margin.l + row_layout.outer_margin.r,
        row_layout.size.height + row_layout.outer_margin.t + row_layout.outer_margin.b);
}

void ui_begin_container(
    ui_wctx_t *wctx, uint32_t width, uint32_t height, ui_rtlb_t outer_margin, ui_rtlb_t inner_margin)
{
    ui_layout_t *current = ui_get_layout(wctx);
    uint32_t available_width = _sub_or_zero(
        _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL), outer_margin.l + outer_margin.r);
    uint32_t available_height = _sub_or_zero(
        _ui_get_available_space(wctx, UI_LAYOUT_VERTICAL), outer_margin.t + outer_margin.b);
    uint32_t layout_width = width == UINT32_MAX ? available_width : width;
    uint32_t layout_height = height == UINT32_MAX ? available_height : height;
    uint8_t fit_width = width == 0;
    uint8_t fit_height = height == 0;

    if (fit_width || layout_width > available_width)
        layout_width = available_width;
    if (fit_height || layout_height > available_height)
        layout_height = available_height;

    ui_push_layout(
        wctx,
        (ui_layout_t){
            .type = UI_LAYOUT_VERTICAL,
            .size = (gfx_area_t){
                .x = current->cursor_pos_x + outer_margin.l,
                .y = current->cursor_pos_y + outer_margin.t,
                .width = layout_width,
                .height = layout_height,
            },
            .inner_margin = inner_margin,
            .outer_margin = outer_margin,
            .cursor_pos_x = current->cursor_pos_x + outer_margin.l + inner_margin.l,
            .cursor_pos_y = current->cursor_pos_y + outer_margin.t + inner_margin.t,
            .padding_x = 0,
            .padding_y = DEFAULT_PADDING,
            .fit_width = fit_width,
            .fit_height = fit_height,
        });
}

void ui_end_container(ui_wctx_t *wctx)
{
    ui_layout_t container_layout = ui_pop_layout(wctx);
    gfx_area_t size = container_layout.size;

    gfx_draw_rect(
        &wctx->framebuffer,
        (gfx_rect_t){
            .x = size.x,
            .y = size.y,
            .width = size.width,
            .height = size.height,
            .border_color = BORDER_COLOR,
            .border_thickness = BORDER_THICKNESS,
        });

    if (size.width > BORDER_THICKNESS * 2 && size.height > BORDER_THICKNESS * 2) {
        gfx_draw_rect(
            &wctx->framebuffer,
            (gfx_rect_t){
                .x = size.x + BORDER_THICKNESS,
                .y = size.y + BORDER_THICKNESS,
                .width = size.width - BORDER_THICKNESS * 2,
                .height = size.height - BORDER_THICKNESS * 2,
                .border_color = BORDER_SHADOW,
                .border_thickness = BORDER_THICKNESS,
            });
    }

    ui_advance_layout(
        wctx,
        container_layout.size.width + container_layout.outer_margin.l
            + container_layout.outer_margin.r,
        container_layout.size.height + container_layout.outer_margin.t
            + container_layout.outer_margin.b);
}

static bool _ui_has_visible_space(ui_wctx_t *wctx)
{
    return _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL) > 0
           && _ui_get_available_space(wctx, UI_LAYOUT_VERTICAL) > 0;
}

void ui_label(ui_wctx_t *wctx, const char *label)
{
    ui_layout_t *layout = ui_get_layout(wctx);
    gfx_area_t text_area = gfx_get_text_area(&_default_font, label);

    if (!_ui_has_visible_space(wctx))
        return;

    gfx_set_clip(
        &wctx->framebuffer,
        (gfx_area_t){
            .x = layout->cursor_pos_x,
            .y = layout->cursor_pos_y,
            .width = _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL),
            .height = _ui_get_available_space(wctx, UI_LAYOUT_VERTICAL),
        });
    gfx_draw_text(
        &wctx->framebuffer,
        &_default_font,
        (gfx_pos_t){layout->cursor_pos_x + text_area.x, layout->cursor_pos_y + text_area.y},
        TEXT_COLOR,
        label);
    gfx_reset_clip(&wctx->framebuffer);

    ui_advance_layout(wctx, text_area.width, text_area.height);
}

bool ui_is_mouse_in_area(ui_wctx_t *wctx, gfx_area_t area)
{
    return wctx->mouse_state.pos_x >= area.x && wctx->mouse_state.pos_x < area.x + area.width
           && wctx->mouse_state.pos_y >= area.y && wctx->mouse_state.pos_y < area.y + area.height;
}

gfx_pos_t ui_get_mouse_pos(ui_wctx_t *wctx)
{
    return (gfx_pos_t){wctx->mouse_state.pos_x, wctx->mouse_state.pos_y};
}

bool ui_is_mouse_button_down(ui_wctx_t *wctx, input_mouse_button_t button)
{
    return wctx->mouse_state.buttons_state & button;
}

bool ui_is_key_down(ui_wctx_t *wctx, input_keyboard_scancode_t keycode)
{
    return wctx->keyboard_keys[keycode] == INPUT_KEYACTION_DOWN
           || wctx->keyboard_keys[keycode] == INPUT_KEYACTION_HOLD;
}

bool ui_is_key_up(ui_wctx_t *wctx, input_keyboard_scancode_t keycode)
{
    return wctx->keyboard_keys[keycode] == INPUT_KEYACTION_UP;
}

bool ui_button(ui_wctx_t *wctx, const char *label)
{
    ui_layout_t *layout = ui_get_layout(wctx);
    gfx_area_t text_area = gfx_get_text_area(&_default_font, label);
    gfx_area_t button_area = {
        .x = layout->cursor_pos_x,
        .y = layout->cursor_pos_y,
        .width = text_area.width + DEFAULT_PADDING * 2,
        .height = text_area.height + DEFAULT_PADDING * 2,
    };

    if (!_ui_has_visible_space(wctx))
        return false;

    gfx_set_clip(
        &wctx->framebuffer,
        (gfx_area_t){
            .x = layout->cursor_pos_x,
            .y = layout->cursor_pos_y,
            .width = _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL),
            .height = _ui_get_available_space(wctx, UI_LAYOUT_VERTICAL),
        });
    gfx_draw_filled_rect(
        &wctx->framebuffer,
        (gfx_rect_t){
            .x = button_area.x,
            .y = button_area.y,
            .width = button_area.width,
            .height = button_area.height,
            .border_color = BORDER_COLOR,
            .border_thickness = BORDER_THICKNESS,
        },
        SECONDARY_BACKGROUND_COLOR);
    gfx_draw_rect(
        &wctx->framebuffer,
        (gfx_rect_t){
            .x = button_area.x + BORDER_THICKNESS,
            .y = button_area.y + BORDER_THICKNESS,
            .width = button_area.width - BORDER_THICKNESS * 2,
            .height = button_area.height - BORDER_THICKNESS * 2,
            .border_color = BORDER_SHADOW,
            .border_thickness = BORDER_THICKNESS,
        });
    gfx_draw_text(
        &wctx->framebuffer,
        &_default_font,
        (gfx_pos_t){
            .x = button_area.x + text_area.x + DEFAULT_PADDING,
            .y = button_area.y + text_area.y + DEFAULT_PADDING,
        },
        TEXT_COLOR,
        label);
    gfx_reset_clip(&wctx->framebuffer);

    ui_advance_layout(wctx, button_area.width, button_area.height);

    if (ui_is_mouse_in_area(wctx, button_area)
        && ui_is_mouse_button_down(wctx, INPUT_MOUSE_BUTTON_LEFT))
        return true;

    return false;
}

ui_textbox_state_t ui_new_textbox_state(char *buffer, size_t buffer_size)
{
    return (ui_textbox_state_t){.buffer = buffer, .buffer_size = buffer_size, .focused = false};
}

void ui_textbox(ui_wctx_t *wctx, ui_textbox_state_t *state)
{
    ui_layout_t *layout = ui_get_layout(wctx);
    gfx_area_t textbox_area = {
        .x = layout->cursor_pos_x,
        .y = layout->cursor_pos_y,
        .width = _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL),
        .height = 12 + DEFAULT_PADDING * 2,
    };

    if (!_ui_has_visible_space(wctx))
        return;

    gfx_set_clip(
        &wctx->framebuffer,
        (gfx_area_t){
            .x = layout->cursor_pos_x,
            .y = layout->cursor_pos_y,
            .width = _ui_get_available_space(wctx, UI_LAYOUT_HORIZONTAL),
            .height = _ui_get_available_space(wctx, UI_LAYOUT_VERTICAL),
        });
    gfx_draw_filled_rect(
        &wctx->framebuffer,
        (gfx_rect_t){
            .x = textbox_area.x,
            .y = textbox_area.y,
            .width = textbox_area.width,
            .height = textbox_area.height,
            .border_color = BORDER_COLOR,
            .border_thickness = BORDER_THICKNESS,
        },
        SECONDARY_BACKGROUND_COLOR);
    gfx_draw_rect(
        &wctx->framebuffer,
        (gfx_rect_t){
            .x = textbox_area.x + BORDER_THICKNESS,
            .y = textbox_area.y + BORDER_THICKNESS,
            .width = textbox_area.width - BORDER_THICKNESS * 2,
            .height = textbox_area.height - BORDER_THICKNESS * 2,
            .border_color = BORDER_SHADOW,
            .border_thickness = BORDER_THICKNESS,
        });
    gfx_reset_clip(&wctx->framebuffer);

    if (!state->focused && ui_is_mouse_button_down(wctx, INPUT_MOUSE_BUTTON_LEFT)
        && ui_is_mouse_in_area(wctx, textbox_area))
        state->focused = true;

    if (state->focused && ui_is_mouse_button_down(wctx, INPUT_MOUSE_BUTTON_LEFT)
        && !ui_is_mouse_in_area(wctx, textbox_area))
        state->focused = false;

    if (state->focused) {
        gfx_draw_line(
            &wctx->framebuffer,
            (gfx_line_t){
                .x1 = textbox_area.x + 5,
                .y1 = textbox_area.y + 4,
                .x2 = textbox_area.x + 5,
                .y2 = textbox_area.y + textbox_area.height - 6,
                .thickness = BORDER_THICKNESS,
            },
            TEXT_COLOR);
    }

    ui_advance_layout(wctx, textbox_area.width, textbox_area.height);
}
