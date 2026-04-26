/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./menu.h"
#include "./icons.h"
#include "./input.h"
#include "./magic.h"
#include "./window.h"

#include <libgfx.h>
#include <stdlib.h>
#include <string.h>

extern gfx_font_t default_font;

static menubar_t *active_menubar = NULL;
static uint8_t previous_buttons = 0;
static menu_t minimized_menu = {.items = NULL, .item_count = 0, .open = false};
static menu_item_t *minimized_menu_items = NULL;
static const char **minimized_menu_titles = NULL;
static size_t minimized_menu_capacity = 0;

// static int32_t menu_hit_test(gfx_context_t *context, const menu_t *menu, uint32_t x, uint32_t y);
// static bool menu_contains_point(gfx_context_t *context, const menu_t *menu, uint32_t x, uint32_t y);

static bool _point_in_rect(uint32_t x, uint32_t y, uint32_t rx, uint32_t ry, uint32_t w, uint32_t h)
{
    return x >= rx && x < (rx + w) && y >= ry && y < (ry + h);
}

static gfx_pos_t _menu_position(const menubar_t *bar, const menu_t *menu)
{
    if (menu == &minimized_menu)
        return (gfx_pos_t){.x = 0, .y = TOP_BAR_HEIGHT - 1};

    uint32_t cursor_x = 0;
    for (size_t i = 0; i < bar->item_count; i++) {
        const menubar_item_t *item = &bar->items[i];
        uint32_t width = gfx_get_text_width(&default_font, item->label) + MENUBAR_ITEM_GAP;
        if (item->menu == menu)
            return (gfx_pos_t){.x = cursor_x, .y = TOP_BAR_HEIGHT - 1};
        cursor_x += width;
    }

    return (gfx_pos_t){.x = 0, .y = TOP_BAR_HEIGHT - 1};
}

static uint32_t _menu_item_height(const menu_item_t *item)
{
    return item->type == MENU_ITEM_SEPARATOR ? MENU_SEPARATOR_HEIGHT : MENU_ITEM_HEIGHT;
}

static uint32_t _menu_measure_width(const menu_t *menu)
{
    uint32_t max_width = 0;
    for (size_t i = 0; i < menu->item_count; i++) {
        const menu_item_t *item = &menu->items[i];
        if (item->type == MENU_ITEM_ACTION) {
            uint32_t width = gfx_get_text_width(&default_font, item->label);
            max_width = width > max_width ? width : max_width;
        }
    }

    uint32_t width = MENUBAR_START_X * 2 + max_width;
    if (width < MENU_MIN_WIDTH)
        width = MENU_MIN_WIDTH;

    return width;
}

static uint32_t _menu_measure_height(const menu_t *menu)
{
    uint32_t height = MENU_VERTICAL_PADDING * 2;
    for (size_t i = 0; i < menu->item_count; i++)
        height += _menu_item_height(&menu->items[i]);
    return height;
}

static void _minimized_icon_rect(
    const gfx_context_t *context, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height)
{
    uint32_t w = background_windows_bitmap.width;
    uint32_t h = background_windows_bitmap.height;
    uint32_t padding = MENUBAR_START_X;
    uint32_t pos_x = (uint32_t) context->width > (w + padding)
                         ? (uint32_t) context->width - w - padding
                         : 0;
    uint32_t pos_y = TOP_BAR_HEIGHT > h ? (TOP_BAR_HEIGHT - h) / 2 : 0;

    if (x)
        *x = pos_x;
    if (y)
        *y = pos_y;
    if (width)
        *width = w;
    if (height)
        *height = h;
}

static bool _minimized_menu_refresh(void)
{
    size_t minimized_count = window_get_minimized_count();
    if (minimized_count == 0) {
        minimized_menu.open = false;
        minimized_menu.item_count = 0;
        return false;
    }

    if (minimized_count > minimized_menu_capacity) {
        menu_item_t *items = malloc(minimized_count * sizeof(*items));
        const char **titles = malloc(minimized_count * sizeof(*titles));
        if (!items || !titles) {
            free(items);
            free(titles);
            minimized_menu.open = false;
            minimized_menu.item_count = 0;
            return false;
        }
        free(minimized_menu_items);
        free(minimized_menu_titles);
        minimized_menu_items = items;
        minimized_menu_titles = titles;
        minimized_menu_capacity = minimized_count;
    }

    size_t filled = window_get_minimized_titles(minimized_menu_titles, minimized_menu_capacity);
    for (size_t i = 0; i < filled; i++) {
        minimized_menu_items[i].label = minimized_menu_titles[i];
        minimized_menu_items[i].type = MENU_ITEM_ACTION;
        minimized_menu_items[i].action = NULL;
    }

    minimized_menu.items = minimized_menu_items;
    minimized_menu.item_count = filled;
    return true;
}

static gfx_pos_t _minimized_menu_position(gfx_context_t *context)
{
    uint32_t menu_width = _menu_measure_width(&minimized_menu);
    uint32_t desired_right = (uint32_t) context->width;
    return (gfx_pos_t){
        .x = desired_right > menu_width ? desired_right - menu_width : 0,
        .y = TOP_BAR_HEIGHT - 1,
    };
}

static int32_t menu_hit_test(gfx_context_t *context, const menu_t *menu, uint32_t x, uint32_t y)
{
    gfx_pos_t menu_pos = menu == &minimized_menu ? _minimized_menu_position(context)
                                                 : _menu_position(active_menubar, menu);

    uint32_t width = _menu_measure_width(menu);
    uint32_t height = _menu_measure_height(menu);
    if (!_point_in_rect(x, y, menu_pos.x, menu_pos.y, width, height)) {
        return -1;
    }

    uint32_t cursor_y = menu_pos.y + MENU_VERTICAL_PADDING;
    for (size_t i = 0; i < menu->item_count; i++) {
        uint32_t item_height = _menu_item_height(&menu->items[i]);
        if (_point_in_rect(x, y, menu_pos.x, cursor_y, width, item_height)) {
            return (int32_t) i;
        }
        cursor_y += item_height;
    }

    return -1;
}

static bool menu_contains_point(gfx_context_t *context, const menu_t *menu, uint32_t x, uint32_t y)
{
    gfx_pos_t menu_pos = menu == &minimized_menu ? _minimized_menu_position(context)
                                                 : _menu_position(active_menubar, menu);

    uint32_t width = _menu_measure_width(menu);
    uint32_t height = _menu_measure_height(menu);
    return _point_in_rect(x, y, menu_pos.x, menu_pos.y, width, height);
}

static bool _minimized_menu_handle_click(gfx_context_t *context, uint32_t x, uint32_t y)
{
    if (!_minimized_menu_refresh()) {
        return false;
    }

    uint32_t icon_x = 0;
    uint32_t icon_y = 0;
    uint32_t icon_w = 0;
    uint32_t icon_h = 0;
    _minimized_icon_rect(context, &icon_x, &icon_y, &icon_w, &icon_h);

    if (_point_in_rect(x, y, icon_x, icon_y, icon_w, icon_h)) {
        minimized_menu.open = !minimized_menu.open;
        return true;
    }

    if (!minimized_menu.open) {
        return false;
    }

    if (menu_contains_point(context, &minimized_menu, x, y)) {
        int32_t hit = menu_hit_test(context, &minimized_menu, x, y);
        if (hit >= 0) {
            window_unminimize_at((size_t) hit);
        }
        minimized_menu.open = false;
        return true;
    }

    minimized_menu.open = false;
    return false;
}

static int32_t _menubar_hit_test(const menubar_t *bar, uint32_t x, uint32_t y)
{
    if (!_point_in_rect(x, y, 0, 0, UINT32_MAX, TOP_BAR_HEIGHT)) {
        return -1;
    }

    uint32_t cursor_x = MENUBAR_START_X;
    for (size_t i = 0; i < bar->item_count; i++) {
        const menubar_item_t *item = &bar->items[i];
        uint32_t width = gfx_get_text_width(&default_font, item->label) + MENUBAR_ITEM_GAP;
        if (_point_in_rect(x, y, cursor_x, 0, width, TOP_BAR_HEIGHT)) {
            return (int32_t) i;
        }
        cursor_x += width;
    }

    return -1;
}

static void menu_draw(gfx_context_t *context, const menu_t *menu)
{
    gfx_pos_t menu_pos = menu == &minimized_menu ? _minimized_menu_position(context)
                                                 : _menu_position(active_menubar, menu);
    uint32_t width = _menu_measure_width(menu);
    uint32_t height = _menu_measure_height(menu);
    gfx_draw_filled_rect(
        context,
        (gfx_rect_t){
            .x = menu_pos.x,
            .y = menu_pos.y,
            .width = width,
            .height = height,
            .border_color = BORDER_COLOR,
            .border_thickness = BORDER_THICKNESS,
        },
        (gfx_color_t){.a = 0xcc, .r = 0x21, .g = 0x21, .b = 0x21});
    gfx_draw_rect(
        context,
        (gfx_rect_t){
            .x = menu_pos.x + BORDER_THICKNESS,
            .y = menu_pos.y + BORDER_THICKNESS,
            .width = width - BORDER_THICKNESS * 2,
            .height = height - BORDER_THICKNESS * 2,
            .border_color = (gfx_color_t){.a = 0x33, .r = 0xff, .g = 0xff, .b = 0xff},
            .border_thickness = BORDER_THICKNESS,
        });

    uint32_t cursor_y = menu_pos.y + MENU_VERTICAL_PADDING;
    for (size_t i = 0; i < menu->item_count; i++) {
        const menu_item_t *item = &menu->items[i];
        uint32_t item_height = _menu_item_height(item);

        if (item->type == MENU_ITEM_SEPARATOR) {
            uint32_t line_y = cursor_y + (item_height / 2);
            uint32_t border_cut = (BORDER_THICKNESS * 3) / 2;
            gfx_draw_line(
                context,
                (gfx_line_t){
                    .x1 = menu_pos.x + BORDER_THICKNESS,
                    .y1 = line_y,
                    .x2 = menu_pos.x + width - border_cut,
                    .y2 = line_y,
                    .thickness = BORDER_THICKNESS,
                },
                (gfx_color_t){.a = 0x33, .r = 0xff, .g = 0xff, .b = 0xff});
        } else {
            gfx_draw_text(
                context,
                &default_font,
                (gfx_pos_t){menu_pos.x + MENUBAR_START_X, cursor_y + 18},
                FONT_COLOR,
                item->label);
        }

        cursor_y += item_height;
    }
}

static void menubar_draw(gfx_context_t *context, const menubar_t *bar)
{
    uint32_t border_cut = (BORDER_THICKNESS * 3) / 2;
    gfx_draw_filled_rect(
        context,
        (gfx_rect_t){
            .x = 0,
            .y = 0,
            .width = context->width,
            .height = TOP_BAR_HEIGHT,
            .border_thickness = BORDER_THICKNESS,
            .border_color = BORDER_COLOR,
        },
        (gfx_color_t){0x80, 0x21, 0x21, 0x21});
    gfx_draw_line(
        context,
        (gfx_line_t){
            .x1 = BORDER_THICKNESS,
            .y1 = BORDER_THICKNESS,
            .x2 = context->width - border_cut,
            .y2 = BORDER_THICKNESS,
            .thickness = BORDER_SHADOW_THICKNESS,
        },
        (gfx_color_t){0x33, 0xFF, 0xFF, 0xFF});

    uint32_t cursor_x = MENUBAR_START_X;
    for (size_t i = 0; i < bar->item_count; i++) {
        const menubar_item_t *item = &bar->items[i];
        gfx_draw_text(
            context, &default_font, (gfx_pos_t){cursor_x, MENUBAR_LABEL_Y}, FONT_COLOR, item->label);
        cursor_x += gfx_get_text_width(&default_font, item->label) + MENUBAR_ITEM_GAP;
    }

    if (_minimized_menu_refresh()) {
        uint32_t icon_x = 0;
        uint32_t icon_y = 0;
        _minimized_icon_rect(context, &icon_x, &icon_y, NULL, NULL);
        gfx_draw_bitmap(context, &background_windows_bitmap, (gfx_pos_t){icon_x, icon_y}, FONT_COLOR);
    }
}

static void menubar_draw_open_menus(gfx_context_t *context, const menubar_t *bar)
{
    for (size_t i = 0; i < bar->item_count; i++) {
        const menubar_item_t *item = &bar->items[i];
        if (item->menu && item->menu->open)
            menu_draw(context, item->menu);
    }

    if (minimized_menu.open && minimized_menu.item_count > 0) {
        menu_draw(context, &minimized_menu);
    }
}

static void menubar_close_all(menubar_t *bar)
{
    for (size_t i = 0; i < bar->item_count; i++) {
        menubar_item_t *item = &bar->items[i];
        if (item->menu) {
            item->menu->open = false;
        }
    }
}

static void menubar_handle_click(gfx_context_t *context, menubar_t *bar, uint32_t x, uint32_t y)
{
    int32_t label_index = _menubar_hit_test(bar, x, y);
    if (label_index >= 0) {
        menubar_item_t *item = &bar->items[label_index];
        if (item->menu) {
            bool next_state = !item->menu->open;
            menubar_close_all(bar);
            item->menu->open = next_state;
        } else {
            menubar_close_all(bar);
        }
        return;
    }

    for (size_t i = 0; i < bar->item_count; i++) {
        menubar_item_t *item = &bar->items[i];
        if (!item->menu || !item->menu->open) {
            continue;
        }

        if (!menu_contains_point(context, item->menu, x, y))
            continue;
        int32_t hit = menu_hit_test(context, item->menu, x, y);
        if (hit >= 0) {
            const menu_item_t *menu_item = &item->menu->items[hit];
            if (menu_item->type == MENU_ITEM_ACTION && menu_item->action) {
                menu_item->action();
            }
            menubar_close_all(bar);
        }
        return;
    }

    menubar_close_all(bar);
}

bool update_menubar_state(gfx_context_t *context)
{
    menubar_t *target = get_active_menubar();
    if (target == NULL)
        return false;

    bool changed = false;

    if (active_menubar == NULL)
        active_menubar = target;

    if (target != active_menubar) {
        menubar_close_all(active_menubar);
        active_menubar = target;
        changed = true;
    }

    input_mouse_event_t mouse = get_mouse_state();
    uint32_t mouse_x = mouse.x < 0 ? 0u : (uint32_t) mouse.x;
    uint32_t mouse_y = mouse.y < 0 ? 0u : (uint32_t) mouse.y;
    bool left_down = (mouse.buttons & 0x01) != 0;
    bool left_pressed = left_down && ((previous_buttons & 0x01) == 0);

    if (left_pressed) {
        bool minimized_handled = _minimized_menu_handle_click(context, mouse_x, mouse_y);
        if (minimized_handled) {
            menubar_close_all(active_menubar);
            previous_buttons = mouse.buttons;
            return true;
        }
        menubar_handle_click(context, active_menubar, mouse_x, mouse_y);
        changed = true;
    }

    previous_buttons = mouse.buttons;
    return changed;
}

void draw_menubar(gfx_context_t *context)
{
    if (!active_menubar)
        return;
    menubar_draw(context, active_menubar);
    menubar_draw_open_menus(context, active_menubar);
}
