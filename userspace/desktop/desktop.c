/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "desktop.h"
#include "font.h"
#include "graphics.h"
#include "input.h"
#include "window.h"
#include <string.h>

/* Taskbar button dimensions */
#define TASKBAR_BUTTON_WIDTH 120
#define TASKBAR_BUTTON_MARGIN 2
#define TASKBAR_START_BUTTON_WIDTH 24

/* Context _menu item height */
#define CONTEXT_MENU_ITEM_HEIGHT 22
#define CONTEXT_MENU_SEPARATOR_HEIGHT 9
#define CONTEXT_MENU_PADDING 3
#define CONTEXT_MENU_CHECKMARK_WIDTH 20
#define CONTEXT_MENU_SHORTCUT_MARGIN 30

/* Menu */
static menu_t _menu;
static bool _menu_open = false;

/* Taskbar button click state */
static int _taskbar_pressed_window = -1; /* Window ID of pressed taskbar button, -1 if none */

/* Desktop _icons */
static desktop_icon_t _icons[MAX_ICONS];
static int _num_icons = 0;

/* Icon dragging state */
static int _dragging_icon = -1; /* Index of icon being dragged, -1 if none */
static int _drag_offset_x = 0;  /* Offset from icon origin to mouse */
static int _drag_offset_y = 0;

/* Context _menu state */
static context_menu_t _context_menu = {0};
static context_menu_builder_t _context_menu_builder = NULL;

/* Cursor bitmap (16x16 arrow) */
static const uint8_t _cursor_data[16][2] = {
    {0x00, 0x00}, /* ................ */
    {0x40, 0x00}, /* .#.............. */
    {0x60, 0x00}, /* .##............. */
    {0x70, 0x00}, /* .###............ */
    {0x78, 0x00}, /* .####........... */
    {0x7C, 0x00}, /* .#####.......... */
    {0x7E, 0x00}, /* .######......... */
    {0x7F, 0x00}, /* .#######........ */
    {0x7F, 0x80}, /* .########....... */
    {0x7C, 0x00}, /* .#####.......... */
    {0x6C, 0x00}, /* .##.##.......... */
    {0x46, 0x00}, /* .#...##......... */
    {0x06, 0x00}, /* .....##......... */
    {0x03, 0x00}, /* ......##........ */
    {0x03, 0x00}, /* ......##........ */
    {0x00, 0x00}, /* ................ */
};

/* Cursor mask (where cursor is visible) */
static const uint8_t _cursor_mask[16][2] = {
    {0xC0, 0x00}, /* ##.............. */
    {0xE0, 0x00}, /* ###............. */
    {0xF0, 0x00}, /* ####............ */
    {0xF8, 0x00}, /* #####........... */
    {0xFC, 0x00}, /* ######.......... */
    {0xFE, 0x00}, /* #######......... */
    {0xFF, 0x00}, /* ########........ */
    {0xFF, 0x80}, /* #########....... */
    {0xFF, 0xC0}, /* ##########...... */
    {0xFF, 0xC0}, /* ##########...... */
    {0xFE, 0x00}, /* #######......... */
    {0xEF, 0x00}, /* ###.####........ */
    {0xCF, 0x00}, /* ##..####........ */
    {0x07, 0x80}, /* .....####....... */
    {0x07, 0x80}, /* .....####....... */
    {0x03, 0x00}, /* ......##........ */
};

/* Resize NS cursor (vertical double arrow) - 16x16 */
static const uint8_t _cursor_resize_ns_data[16][2] = {
    {0x01, 0x80}, /* .......##....... */
    {0x03, 0xC0}, /* ......####...... */
    {0x07, 0xE0}, /* .....######..... */
    {0x0F, 0xF0}, /* ....########.... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x01, 0x80}, /* .......##....... */
    {0x0F, 0xF0}, /* ....########.... */
    {0x07, 0xE0}, /* .....######..... */
    {0x03, 0xC0}, /* ......####...... */
    {0x01, 0x80}, /* .......##....... */
};

static const uint8_t _cursor_resize_ns_mask[16][2] = {
    {0x03, 0xC0}, /* ......####...... */
    {0x07, 0xE0}, /* .....######..... */
    {0x0F, 0xF0}, /* ....########.... */
    {0x1F, 0xF8}, /* ...##########... */
    {0x1F, 0xF8}, /* ...##########... */
    {0x03, 0xC0}, /* ......####...... */
    {0x03, 0xC0}, /* ......####...... */
    {0x03, 0xC0}, /* ......####...... */
    {0x03, 0xC0}, /* ......####...... */
    {0x03, 0xC0}, /* ......####...... */
    {0x03, 0xC0}, /* ......####...... */
    {0x1F, 0xF8}, /* ...##########... */
    {0x1F, 0xF8}, /* ...##########... */
    {0x0F, 0xF0}, /* ....########.... */
    {0x07, 0xE0}, /* .....######..... */
    {0x03, 0xC0}, /* ......####...... */
};

/* Resize EW cursor (horizontal double arrow) - 16x16 */
static const uint8_t _cursor_resize_ew_data[16][2] = {
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x04, 0x20}, /* .....#....#..... */
    {0x0C, 0x30}, /* ....##....##.... */
    {0x1C, 0x38}, /* ...###....###... */
    {0x3F, 0xFC}, /* ..############.. */
    {0x7F, 0xFE}, /* .##############. */
    {0x3F, 0xFC}, /* ..############.. */
    {0x1C, 0x38}, /* ...###....###... */
    {0x0C, 0x30}, /* ....##....##.... */
    {0x04, 0x20}, /* .....#....#..... */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
};

static const uint8_t _cursor_resize_ew_mask[16][2] = {
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x04, 0x20}, /* .....#....#..... */
    {0x0E, 0x70}, /* ....###..###.... */
    {0x1E, 0x78}, /* ...####..####... */
    {0x3F, 0xFC}, /* ..############.. */
    {0x7F, 0xFE}, /* .##############. */
    {0xFF, 0xFF}, /* ################ */
    {0x7F, 0xFE}, /* .##############. */
    {0x3F, 0xFC}, /* ..############.. */
    {0x1E, 0x78}, /* ...####..####... */
    {0x0E, 0x70}, /* ....###..###.... */
    {0x04, 0x20}, /* .....#....#..... */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
    {0x00, 0x00}, /* ................ */
};

/* Resize NWSE cursor (diagonal \ double arrow) - 16x16 */
static const uint8_t _cursor_resize_nwse_data[16][2] = {
    {0x00, 0x00}, /* ................ */
    {0x7C, 0x00}, /* .#####.......... */
    {0x78, 0x00}, /* .####........... */
    {0x70, 0x00}, /* .###............ */
    {0x68, 0x00}, /* .##.#........... */
    {0x44, 0x00}, /* .#...#.......... */
    {0x02, 0x00}, /* ......#......... */
    {0x01, 0x00}, /* .......#........ */
    {0x00, 0x80}, /* ........#....... */
    {0x00, 0x40}, /* .........#...... */
    {0x00, 0x22}, /* ..........#...#. */
    {0x00, 0x16}, /* ...........#.##. */
    {0x00, 0x0E}, /* ............###. */
    {0x00, 0x1E}, /* ...........####. */
    {0x00, 0x3E}, /* ..........#####. */
    {0x00, 0x00}, /* ................ */
};

static const uint8_t _cursor_resize_nwse_mask[16][2] = {
    {0xFF, 0x00}, /* ########........ */
    {0xFE, 0x00}, /* #######......... */
    {0xFC, 0x00}, /* ######.......... */
    {0xF8, 0x00}, /* #####........... */
    {0xFC, 0x00}, /* ######.......... */
    {0xEE, 0x00}, /* ###.###......... */
    {0xC7, 0x00}, /* ##...###........ */
    {0x83, 0x80}, /* #.....###....... */
    {0x01, 0xC1}, /* .......###.....# */
    {0x00, 0xE3}, /* ........###...## */
    {0x00, 0x77}, /* .........###.### */
    {0x00, 0x3F}, /* ..........###### */
    {0x00, 0x1F}, /* ...........##### */
    {0x00, 0x3F}, /* ..........###### */
    {0x00, 0x7F}, /* .........####### */
    {0x00, 0xFF}, /* ........######## */
};

/* Resize NESW cursor (diagonal / double arrow) - 16x16 */
static const uint8_t _cursor_resize_nesw_data[16][2] = {
    {0x00, 0x00}, /* ................ */
    {0x00, 0x3E}, /* ..........#####. */
    {0x00, 0x1E}, /* ...........####. */
    {0x00, 0x0E}, /* ............###. */
    {0x00, 0x16}, /* ...........#.##. */
    {0x00, 0x22}, /* ..........#...#. */
    {0x00, 0x40}, /* .........#...... */
    {0x00, 0x80}, /* ........#....... */
    {0x01, 0x00}, /* .......#........ */
    {0x02, 0x00}, /* ......#......... */
    {0x44, 0x00}, /* .#...#,......... */
    {0x68, 0x00}, /* .##.#........... */
    {0x70, 0x00}, /* .###............ */
    {0x78, 0x00}, /* .####........... */
    {0x7C, 0x00}, /* .#####.......... */
    {0x00, 0x00}, /* ................ */
};

static const uint8_t _cursor_resize_nesw_mask[16][2] = {
    {0x00, 0xFF}, /* ........######## */
    {0x00, 0x7F}, /* .........####### */
    {0x00, 0x3F}, /* ..........###### */
    {0x00, 0x1F}, /* ...........##### */
    {0x00, 0x3F}, /* ..........###### */
    {0x00, 0x77}, /* .........###.### */
    {0x00, 0xE3}, /* ........###...## */
    {0x01, 0xC1}, /* .......###.....# */
    {0x83, 0x80}, /* #.....###....... */
    {0xC7, 0x00}, /* ##...###........ */
    {0xEE, 0x00}, /* ###.###......... */
    {0xFC, 0x00}, /* ######.......... */
    {0xF8, 0x00}, /* #####........... */
    {0xFC, 0x00}, /* ######.......... */
    {0xFE, 0x00}, /* #######......... */
    {0xFF, 0x00}, /* ########........ */
};

/* Current cursor type */
static cursor_type_t _current_cursor = CURSOR_ARROW;

/* Menu icon bitmap (hamburger _menu) - 8x10 */
static const uint8_t _menu_icon[10] = {
    0x00, /* ........ */
    0x00, /* ........ */
    0x7E, /* .######. */
    0x00, /* ........ */
    0x7E, /* .######. */
    0x00, /* ........ */
    0x7E, /* .######. */
    0x00, /* ........ */
    0x00, /* ........ */
    0x00, /* ........ */
};

static bool _point_in_rect(int x, int y, rect_t r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int _get_menu_dropdown_width(menu_t *menu)
{
    int max_width = 0;
    for (int i = 0; i < menu->num_items; i++) {
        if (!menu->items[i].separator) {
            int w = font_text_width(menu->items[i].label);
            if (w > max_width)
                max_width = w;
        }
    }
    return max_width + 40; /* Padding */
}

static void _draw_dropdown_menu(void)
{
    if (!_menu_open)
        return;

    int dropdown_w = _get_menu_dropdown_width(&_menu);
    int dropdown_h = _menu.num_items * 20 + 4;

    /* Position dropdown below start button */
    int dropdown_x = 2;

    rect_t dropdown = {dropdown_x, TASKBAR_HEIGHT, dropdown_w, dropdown_h};

    /* Draw shadow (dark theme - subtle shadow) */
    rect_t shadow = {dropdown.x + 3, dropdown.y + 3, dropdown.w, dropdown.h};
    graphics_fill_rect(shadow, COLOR_BLACK);

    /* Draw _menu background - dark with 3D effect */
    graphics_fill_rect(dropdown, COLOR_PLATINUM);

    /* 3D border effect */
    graphics_hline(dropdown.x, dropdown.x + dropdown.w - 1, dropdown.y, COLOR_BEVEL_LIGHT);
    graphics_vline(dropdown.x, dropdown.y, dropdown.y + dropdown.h - 1, COLOR_BEVEL_LIGHT);
    graphics_hline(
        dropdown.x, dropdown.x + dropdown.w - 1, dropdown.y + dropdown.h - 1, COLOR_BEVEL_DARK);
    graphics_vline(
        dropdown.x + dropdown.w - 1, dropdown.y, dropdown.y + dropdown.h - 1, COLOR_BEVEL_DARK);

    /* Outer frame */
    graphics_draw_rect(dropdown, COLOR_BLACK);

    /* Draw items */
    int y = TASKBAR_HEIGHT + 2;
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();

    for (int i = 0; i < _menu.num_items; i++) {
        menu_item_t *item = &_menu.items[i];

        if (item->separator) {
            /* Draw separator line - dark theme etched line */
            graphics_hline(dropdown.x + 2, dropdown.x + dropdown.w - 3, y + 9, COLOR_BEVEL_DARK);
            graphics_hline(dropdown.x + 2, dropdown.x + dropdown.w - 3, y + 10, COLOR_BEVEL_LIGHT);
        } else {
            /* Check hover */
            rect_t item_rect = {dropdown.x + 2, y + 1, dropdown.w - 4, 18};
            bool hover = _point_in_rect(mx, my, item_rect);

            if (hover && item->enabled) {
                graphics_fill_rect(item_rect, COLOR_HIGHLIGHT);
                graphics_draw_text(item->label, dropdown.x + 8, y + 3, COLOR_WHITE);
            } else {
                graphics_draw_text(
                    item->label, dropdown.x + 8, y + 3, item->enabled ? COLOR_TEXT : COLOR_TEXT_DIM);
            }
        }

        y += 20;
    }
}

static void _draw_folder_icon(int x, int y, bool selected)
{
    /* Yellow folder colors */
    color_t folder_color = COLOR_MAKE(220, 180, 60, 255);  /* Yellow folder */
    color_t folder_dark = COLOR_MAKE(180, 140, 40, 255);   /* Darker shade */
    color_t folder_light = COLOR_MAKE(250, 210, 100, 255); /* Lighter shade */
    color_t outline = COLOR_MAKE(140, 110, 30, 255);

    if (selected) {
        /* Brighter when selected */
        folder_color = COLOR_MAKE(255, 210, 80, 255); /* Highlight yellow */
        folder_dark = COLOR_MAKE(200, 160, 50, 255);
        folder_light = COLOR_MAKE(255, 235, 140, 255);
        outline = COLOR_WHITE;
    }

    /* Tab on top */
    rect_t tab = {x + 2, y, 14, 6};
    graphics_fill_rect(tab, folder_color);
    graphics_hline(x + 2, x + 15, y, folder_light); /* Top highlight */
    graphics_vline(x + 2, y, y + 5, folder_light);  /* Left highlight */
    graphics_draw_rect(tab, outline);

    /* Main folder body */
    rect_t body = {x, y + 4, ICON_WIDTH, ICON_HEIGHT - 4};
    graphics_fill_rect(body, folder_color);

    /* 3D shading on folder body */
    graphics_hline(x + 1, x + ICON_WIDTH - 2, y + 5, folder_light);  /* Top highlight */
    graphics_vline(x + 1, y + 5, y + ICON_HEIGHT - 2, folder_light); /* Left highlight */
    graphics_hline(x + 1, x + ICON_WIDTH - 2, y + ICON_HEIGHT - 2, folder_dark); /* Bottom shadow */
    graphics_vline(x + ICON_WIDTH - 2, y + 5, y + ICON_HEIGHT - 2, folder_dark); /* Right shadow */

    /* Outline */
    graphics_draw_rect(body, outline);

    /* Inner fold line */
    graphics_hline(x + 2, x + ICON_WIDTH - 3, y + 9, folder_dark);
}

static void _draw_icons(void)
{
    for (int i = 0; i < _num_icons; i++) {
        desktop_icon_t *icon = &_icons[i];

        /* Draw icon */
        _draw_folder_icon(icon->x, icon->y, icon->selected);

        /* Draw label with dark theme style */
        int label_w = font_text_width(icon->label);
        int label_x = icon->x + (ICON_WIDTH - label_w) / 2;
        int label_y = icon->y + ICON_HEIGHT + ICON_TEXT_OFFSET;

        if (icon->selected) {
            /* Blue highlight background for selected text */
            rect_t label_bg = {label_x - 3, label_y - 1, label_w + 6, font_text_height() + 2};
            graphics_fill_rect(label_bg, COLOR_HIGHLIGHT);
            graphics_draw_text(icon->label, label_x, label_y, COLOR_WHITE);
        } else {
            /* Light text on dark background */
            graphics_draw_text(icon->label, label_x, label_y, COLOR_TEXT);
        }
    }
}

static void draw_menu_icon(int x, int y)
{
    for (int row = 0; row < 10; row++) {
        uint8_t bits = _menu_icon[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                graphics_pixel(x + col, y + row, COLOR_TEXT);
            }
        }
    }
}

static void _draw_taskbar(void)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Taskbar background - dark theme */
    rect_t taskbar = {0, 0, (int) fb->width, TASKBAR_HEIGHT};
    graphics_fill_rect(taskbar, COLOR_PLATINUM);

    /* Top highlight */
    graphics_hline(0, (int) fb->width - 1, 0, COLOR_BEVEL_LIGHT);

    /* Taskbar bottom border with shadow */
    graphics_hline(0, (int) fb->width - 1, TASKBAR_HEIGHT - 2, COLOR_BEVEL_DARK);
    graphics_hline(0, (int) fb->width - 1, TASKBAR_HEIGHT - 1, COLOR_BLACK);

    /* Draw start/_menu button with 3D effect */
    rect_t start_btn = {2, 3, TASKBAR_START_BUTTON_WIDTH, TASKBAR_HEIGHT - 6};
    bool start_pressed = _menu_open;

    if (start_pressed) {
        graphics_fill_rect(start_btn, COLOR_DARK_GRAY);
        /* Inner bevel for pressed state */
        graphics_hline(start_btn.x + 1, start_btn.x + start_btn.w - 2, start_btn.y + 1, COLOR_BLACK);
        graphics_vline(start_btn.x + 1, start_btn.y + 1, start_btn.y + start_btn.h - 2, COLOR_BLACK);
        graphics_hline(
            start_btn.x + 1,
            start_btn.x + start_btn.w - 2,
            start_btn.y + start_btn.h - 2,
            COLOR_BEVEL_LIGHT);
        graphics_vline(
            start_btn.x + start_btn.w - 2,
            start_btn.y + 1,
            start_btn.y + start_btn.h - 2,
            COLOR_BEVEL_LIGHT);
    } else {
        graphics_fill_rect(start_btn, COLOR_PLATINUM);
        /* Inner bevel for raised state */
        graphics_hline(
            start_btn.x + 1, start_btn.x + start_btn.w - 2, start_btn.y + 1, COLOR_BEVEL_LIGHT);
        graphics_vline(
            start_btn.x + 1, start_btn.y + 1, start_btn.y + start_btn.h - 2, COLOR_BEVEL_LIGHT);
        graphics_hline(
            start_btn.x + 1,
            start_btn.x + start_btn.w - 2,
            start_btn.y + start_btn.h - 2,
            COLOR_BEVEL_DARK);
        graphics_vline(
            start_btn.x + start_btn.w - 2,
            start_btn.y + 1,
            start_btn.y + start_btn.h - 2,
            COLOR_BEVEL_DARK);
    }
    graphics_draw_rect(start_btn, COLOR_BLACK);

    /* Draw _menu icon centered in start button */
    draw_menu_icon(start_btn.x + (start_btn.w - 8) / 2, start_btn.y + (start_btn.h - 10) / 2);

    /* Separator after start button */
    int sep_x = TASKBAR_START_BUTTON_WIDTH + 6;
    graphics_vline(sep_x, 3, TASKBAR_HEIGHT - 4, COLOR_BEVEL_DARK);
    graphics_vline(sep_x + 1, 3, TASKBAR_HEIGHT - 4, COLOR_BEVEL_LIGHT);

    /* Draw window buttons */
    int btn_x = sep_x + 6;
    int num_windows = wm_get_window_count();
    window_t *active_win = wm_get_active();
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();

    for (int i = 0; i < num_windows; i++) {
        window_t *win = wm_get_window_by_creation_order(i);
        if (!win)
            continue;

        /* Calculate button width - shrink if too many windows */
        int available_width = (int) fb->width - btn_x - 10;
        int max_buttons = available_width / (TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_MARGIN);
        int btn_width = TASKBAR_BUTTON_WIDTH;
        if (num_windows > max_buttons && max_buttons > 0) {
            btn_width = (available_width / num_windows) - TASKBAR_BUTTON_MARGIN;
            if (btn_width < 40)
                btn_width = 40;
        }

        rect_t btn = {btn_x, 3, btn_width, TASKBAR_HEIGHT - 6};
        bool is_active = (win == active_win) && !(win->flags & WINDOW_FLAG_MINIMIZED);
        bool is_minimized = (win->flags & WINDOW_FLAG_MINIMIZED) != 0;
        bool is_pressed = (_taskbar_pressed_window == win->id);
        bool is_hovered = (mx >= btn.x && mx < btn.x + btn.w && my >= btn.y && my < btn.y + btn.h);

        /* Draw button with distinct states */
        if (is_pressed && is_hovered) {
            /* Pressed state - deeply sunken */
            graphics_fill_rect(btn, COLOR_MAKE(35, 35, 40, 255));
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + 1, COLOR_BLACK);
            graphics_vline(btn.x + 1, btn.y + 1, btn.y + btn.h - 2, COLOR_BLACK);
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + btn.h - 2, COLOR_BEVEL_LIGHT);
            graphics_vline(btn.x + btn.w - 2, btn.y + 1, btn.y + btn.h - 2, COLOR_BEVEL_LIGHT);
        } else if (is_active) {
            /* Active/focused window - sunken with highlight color */
            graphics_fill_rect(btn, COLOR_MAKE(50, 70, 100, 255)); /* Subtle blue tint */
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + 1, COLOR_BEVEL_DARK);
            graphics_vline(btn.x + 1, btn.y + 1, btn.y + btn.h - 2, COLOR_BEVEL_DARK);
            graphics_hline(
                btn.x + 1, btn.x + btn.w - 2, btn.y + btn.h - 2, COLOR_MAKE(70, 90, 120, 255));
            graphics_vline(
                btn.x + btn.w - 2, btn.y + 1, btn.y + btn.h - 2, COLOR_MAKE(70, 90, 120, 255));
        } else if (is_minimized) {
            /* Minimized window - flatter, dimmer appearance */
            graphics_fill_rect(btn, COLOR_MAKE(50, 50, 55, 255));
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + 1, COLOR_MAKE(60, 60, 65, 255));
            graphics_vline(btn.x + 1, btn.y + 1, btn.y + btn.h - 2, COLOR_MAKE(60, 60, 65, 255));
            graphics_hline(
                btn.x + 1, btn.x + btn.w - 2, btn.y + btn.h - 2, COLOR_MAKE(40, 40, 45, 255));
            graphics_vline(
                btn.x + btn.w - 2, btn.y + 1, btn.y + btn.h - 2, COLOR_MAKE(40, 40, 45, 255));
        } else {
            /* Inactive but visible window - raised look */
            graphics_fill_rect(btn, COLOR_PLATINUM);
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + 1, COLOR_BEVEL_LIGHT);
            graphics_vline(btn.x + 1, btn.y + 1, btn.y + btn.h - 2, COLOR_BEVEL_LIGHT);
            graphics_hline(btn.x + 1, btn.x + btn.w - 2, btn.y + btn.h - 2, COLOR_BEVEL_DARK);
            graphics_vline(btn.x + btn.w - 2, btn.y + 1, btn.y + btn.h - 2, COLOR_BEVEL_DARK);
        }
        graphics_draw_rect(btn, COLOR_BLACK);

        /* Draw window title (truncated if needed) */
        int max_title_width = btn_width - 8;
        char title[32];
        strncpy(title, win->title, 31);
        title[31] = '\0';

        /* Truncate with ellipsis if too long */
        while (font_text_width(title) > max_title_width && strlen(title) > 3) {
            title[strlen(title) - 1] = '\0';
            int len = strlen(title);
            if (len >= 3) {
                title[len - 1] = '.';
                title[len - 2] = '.';
                title[len - 3] = '.';
            }
        }

        int text_y = btn.y + (btn.h - font_text_height()) / 2;
        color_t text_color;
        if (is_active) {
            text_color = COLOR_WHITE;
        } else if (is_minimized) {
            text_color = COLOR_TEXT_DIM;
        } else {
            text_color = COLOR_TEXT;
        }
        graphics_draw_text(title, btn.x + 4, text_y, text_color);

        btn_x += btn_width + TASKBAR_BUTTON_MARGIN;
    }
}

/* Calculate the dimensions of the context menu */
static void _get_context_menu_dimensions(int *width, int *height)
{
    int max_label_width = 0;
    int max_shortcut_width = 0;
    int total_height = CONTEXT_MENU_PADDING * 2;

    for (int i = 0; i < _context_menu.num_items; i++) {
        context_menu_item_t *item = &_context_menu.items[i];

        if (item->separator) {
            total_height += CONTEXT_MENU_SEPARATOR_HEIGHT;
        } else {
            total_height += CONTEXT_MENU_ITEM_HEIGHT;

            int label_w = font_text_width(item->label);
            if (label_w > max_label_width)
                max_label_width = label_w;

            if (item->shortcut[0] != '\0') {
                int shortcut_w = font_text_width(item->shortcut);
                if (shortcut_w > max_shortcut_width)
                    max_shortcut_width = shortcut_w;
            }
        }
    }

    /* Width = checkmark area + label + margin + shortcut + padding */
    *width = CONTEXT_MENU_CHECKMARK_WIDTH + max_label_width + CONTEXT_MENU_SHORTCUT_MARGIN
             + max_shortcut_width + CONTEXT_MENU_PADDING * 2;
    if (*width < 120)
        *width = 120; /* Minimum width */

    *height = total_height;
}

/* Draw checkmark glyph (Windows-style) */
static void _draw_checkmark(int x, int y, color_t color)
{
    /* Simple checkmark shape */
    graphics_pixel(x + 2, y + 5, color);
    graphics_pixel(x + 3, y + 6, color);
    graphics_pixel(x + 4, y + 7, color);
    graphics_pixel(x + 5, y + 8, color);
    graphics_pixel(x + 6, y + 7, color);
    graphics_pixel(x + 7, y + 6, color);
    graphics_pixel(x + 8, y + 5, color);
    graphics_pixel(x + 9, y + 4, color);
    graphics_pixel(x + 10, y + 3, color);

    /* Make it thicker */
    graphics_pixel(x + 2, y + 6, color);
    graphics_pixel(x + 3, y + 7, color);
    graphics_pixel(x + 4, y + 8, color);
    graphics_pixel(x + 5, y + 9, color);
    graphics_pixel(x + 6, y + 8, color);
    graphics_pixel(x + 7, y + 7, color);
    graphics_pixel(x + 8, y + 6, color);
    graphics_pixel(x + 9, y + 5, color);
    graphics_pixel(x + 10, y + 4, color);
}

/* Draw the context menu - Windows-like styling */
static void _draw_context_menu(void)
{
    if (!_context_menu.visible || _context_menu.num_items == 0)
        return;

    int menu_w, menu_h;
    _get_context_menu_dimensions(&menu_w, &menu_h);

    rect_t menu_rect = {_context_menu.x, _context_menu.y, menu_w, menu_h};

    /* Draw shadow (offset by 2,2 for depth effect) */
    rect_t shadow = {menu_rect.x + 2, menu_rect.y + 2, menu_rect.w, menu_rect.h};
    graphics_fill_rect(shadow, COLOR_MAKE(0, 0, 0, 180));

    /* Draw _menu background - slightly lighter than taskbar for distinction */
    color_t menu_bg = COLOR_MAKE(55, 55, 60, 255);
    graphics_fill_rect(menu_rect, menu_bg);

    /* Draw 3D border effect - Windows style raised edge */
    /* Outer light edge (top and left) */
    graphics_hline(menu_rect.x, menu_rect.x + menu_rect.w - 1, menu_rect.y, COLOR_BEVEL_LIGHT);
    graphics_vline(menu_rect.x, menu_rect.y, menu_rect.y + menu_rect.h - 1, COLOR_BEVEL_LIGHT);

    /* Outer dark edge (bottom and right) */
    graphics_hline(
        menu_rect.x, menu_rect.x + menu_rect.w - 1, menu_rect.y + menu_rect.h - 1, COLOR_BLACK);
    graphics_vline(
        menu_rect.x + menu_rect.w - 1, menu_rect.y, menu_rect.y + menu_rect.h - 1, COLOR_BLACK);

    /* Inner light edge (bottom and right - gives 3D depth) */
    graphics_hline(
        menu_rect.x + 1,
        menu_rect.x + menu_rect.w - 2,
        menu_rect.y + menu_rect.h - 2,
        COLOR_BEVEL_DARK);
    graphics_vline(
        menu_rect.x + menu_rect.w - 2,
        menu_rect.y + 1,
        menu_rect.y + menu_rect.h - 2,
        COLOR_BEVEL_DARK);

    /* Draw items */
    int y = _context_menu.y + CONTEXT_MENU_PADDING;
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();

    for (int i = 0; i < _context_menu.num_items; i++) {
        context_menu_item_t *item = &_context_menu.items[i];

        if (item->separator) {
            /* Draw separator - Windows style etched line */
            int sep_y = y + CONTEXT_MENU_SEPARATOR_HEIGHT / 2;
            graphics_hline(menu_rect.x + 3, menu_rect.x + menu_rect.w - 4, sep_y, COLOR_BEVEL_DARK);
            graphics_hline(
                menu_rect.x + 3, menu_rect.x + menu_rect.w - 4, sep_y + 1, COLOR_BEVEL_LIGHT);
            y += CONTEXT_MENU_SEPARATOR_HEIGHT;
        } else {
            /* Calculate item rect for hover detection */
            rect_t item_rect = {menu_rect.x + 2, y, menu_rect.w - 4, CONTEXT_MENU_ITEM_HEIGHT};

            bool hover = _point_in_rect(mx, my, item_rect) && item->enabled;

            /* Draw item background on hover */
            if (hover) {
                graphics_fill_rect(item_rect, COLOR_HIGHLIGHT);
                _context_menu.hover_index = i;
            }

            /* Determine text color */
            color_t text_color;
            if (!item->enabled) {
                text_color = COLOR_TEXT_DIM;
            } else if (hover) {
                text_color = COLOR_WHITE;
            } else {
                text_color = COLOR_TEXT;
            }

            /* Draw checkmark if checked */
            if (item->checked) {
                _draw_checkmark(menu_rect.x + 4, y + 3, text_color);
            }

            /* Draw label */
            int text_x = menu_rect.x + CONTEXT_MENU_CHECKMARK_WIDTH;
            int text_y = y + (CONTEXT_MENU_ITEM_HEIGHT - font_text_height()) / 2;
            graphics_draw_text(item->label, text_x, text_y, text_color);

            /* Draw shortcut (right-aligned, dimmer) */
            if (item->shortcut[0] != '\0') {
                int shortcut_w = font_text_width(item->shortcut);
                int shortcut_x = menu_rect.x + menu_rect.w - shortcut_w - CONTEXT_MENU_PADDING - 3;
                color_t shortcut_color = hover ? COLOR_WHITE : COLOR_TEXT_DIM;
                graphics_draw_text(item->shortcut, shortcut_x, text_y, shortcut_color);
            }

            y += CONTEXT_MENU_ITEM_HEIGHT;
        }
    }
}

/* Process context _menu input - returns true if context _menu consumed the click */
static bool _process_context_menu_input(void)
{
    if (!_context_menu.visible)
        return false;

    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool clicked = input_mouse_left_clicked();

    int menu_w, menu_h;
    _get_context_menu_dimensions(&menu_w, &menu_h);

    rect_t menu_rect = {_context_menu.x, _context_menu.y, menu_w, menu_h};

    if (clicked) {
        if (_point_in_rect(mx, my, menu_rect)) {
            /* Find which item was clicked */
            int y = _context_menu.y + CONTEXT_MENU_PADDING;

            for (int i = 0; i < _context_menu.num_items; i++) {
                context_menu_item_t *item = &_context_menu.items[i];

                if (item->separator) {
                    y += CONTEXT_MENU_SEPARATOR_HEIGHT;
                } else {
                    rect_t item_rect
                        = {menu_rect.x + 2, y, menu_rect.w - 4, CONTEXT_MENU_ITEM_HEIGHT};

                    if (_point_in_rect(mx, my, item_rect)) {
                        if (item->enabled && item->callback) {
                            item->callback();
                        }
                        desktop_close_context_menu();
                        return true;
                    }

                    y += CONTEXT_MENU_ITEM_HEIGHT;
                }
            }
        }

        /* Clicked outside _menu - close it */
        desktop_close_context_menu();
        return true; /* Consume the click so nothing else handles it */
    }

    return false;
}

static void _draw_cursor_bitmap(
    int mx,
    int my,
    const uint8_t data[16][2],
    const uint8_t mask[16][2],
    int hotspot_x,
    int hotspot_y)
{
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int byte_idx = col / 8;
            int bit_idx = 7 - (col % 8);

            uint8_t mask_bit = (mask[row][byte_idx] >> bit_idx) & 1;
            uint8_t data_bit = (data[row][byte_idx] >> bit_idx) & 1;

            if (mask_bit) {
                color_t c = data_bit ? COLOR_BLACK : COLOR_WHITE;
                graphics_pixel(mx + col - hotspot_x, my + row - hotspot_y, c);
            }
        }
    }
}

static void _desktop_add_icon(const char *label, int x, int y)
{
    if (_num_icons >= MAX_ICONS)
        return;

    desktop_icon_t *icon = &_icons[_num_icons++];
    strncpy(icon->label, label, 31);
    icon->label[31] = '\0';
    icon->x = x;
    icon->y = y;
    icon->selected = false;
}

/* Check if a grid cell is occupied by any icon */
static bool _is_grid_cell_occupied(int grid_x, int grid_y)
{
    int cell_x = ICON_GRID_START_X + grid_x * ICON_GRID_CELL_WIDTH;
    int cell_y = ICON_GRID_START_Y + grid_y * ICON_GRID_CELL_HEIGHT;

    for (int i = 0; i < _num_icons; i++) {
        /* Check if icon center is within this cell */
        int icon_cx = _icons[i].x + ICON_WIDTH / 2;
        int icon_cy = _icons[i].y + ICON_HEIGHT / 2;

        if (icon_cx >= cell_x && icon_cx < cell_x + ICON_GRID_CELL_WIDTH && icon_cy >= cell_y
            && icon_cy < cell_y + ICON_GRID_CELL_HEIGHT) {
            return true;
        }
    }
    return false;
}

static void _desktop_get_next_grid_position(int *x, int *y)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Calculate how many rows and columns fit on the desktop */
    int desktop_height = (int) fb->height - ICON_GRID_START_Y - 20;
    int desktop_width = (int) fb->width - ICON_GRID_START_X - 20;

    int max_rows = desktop_height / ICON_GRID_CELL_HEIGHT;
    int max_cols = desktop_width / ICON_GRID_CELL_WIDTH;

    if (max_rows < 1)
        max_rows = 1;
    if (max_cols < 1)
        max_cols = 1;

    /* Find first empty cell, scanning column by column (like Windows) */
    for (int col = 0; col < max_cols; col++) {
        for (int row = 0; row < max_rows; row++) {
            if (!_is_grid_cell_occupied(col, row)) {
                *x = ICON_GRID_START_X + col * ICON_GRID_CELL_WIDTH
                     + (ICON_GRID_CELL_WIDTH - ICON_WIDTH) / 2;
                *y = ICON_GRID_START_Y + row * ICON_GRID_CELL_HEIGHT;
                return;
            }
        }
    }

    /* All cells occupied, use first position */
    *x = ICON_GRID_START_X + (ICON_GRID_CELL_WIDTH - ICON_WIDTH) / 2;
    *y = ICON_GRID_START_Y;
}

static void _desktop_snap_to_grid(int *x, int *y)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Calculate which grid cell this position is closest to */
    int rel_x = *x - ICON_GRID_START_X + ICON_GRID_CELL_WIDTH / 2;
    int rel_y = *y - ICON_GRID_START_Y + ICON_GRID_CELL_HEIGHT / 2;

    int grid_col = rel_x / ICON_GRID_CELL_WIDTH;
    int grid_row = rel_y / ICON_GRID_CELL_HEIGHT;

    /* Clamp to valid range */
    if (grid_col < 0)
        grid_col = 0;
    if (grid_row < 0)
        grid_row = 0;

    int max_cols = ((int) fb->width - ICON_GRID_START_X - 20) / ICON_GRID_CELL_WIDTH;
    int max_rows = ((int) fb->height - ICON_GRID_START_Y - 20) / ICON_GRID_CELL_HEIGHT;

    if (grid_col >= max_cols)
        grid_col = max_cols - 1;
    if (grid_row >= max_rows)
        grid_row = max_rows - 1;

    /* Calculate snapped position (centered in cell) */
    *x = ICON_GRID_START_X + grid_col * ICON_GRID_CELL_WIDTH
         + (ICON_GRID_CELL_WIDTH - ICON_WIDTH) / 2;
    *y = ICON_GRID_START_Y + grid_row * ICON_GRID_CELL_HEIGHT;
}

void desktop_init(void)
{
    memset(&_menu, 0, sizeof(_menu));
    memset(_icons, 0, sizeof(_icons));
    memset(&_context_menu, 0, sizeof(_context_menu));
    _num_icons = 0;
    _menu_open = false;
    _context_menu.visible = false;
    _context_menu.hover_index = -1;
    _context_menu_builder = NULL;
}

void desktop_add_menu_item(const char *label, void (*callback)(void))
{
    if (_menu.num_items >= MAX_MENU_ITEMS)
        return;

    menu_item_t *item = &_menu.items[_menu.num_items++];
    strncpy(item->label, label, 31);
    item->label[31] = '\0';
    item->separator = false;
    item->enabled = true;
    item->callback = callback;
}

void desktop_add_menu_separator(void)
{
    if (_menu.num_items >= MAX_MENU_ITEMS)
        return;

    menu_item_t *item = &_menu.items[_menu.num_items++];
    item->separator = true;
    item->enabled = false;
    item->callback = NULL;
}

void desktop_add_icon_grid(const char *label)
{
    if (_num_icons >= MAX_ICONS)
        return;

    int x, y;
    _desktop_get_next_grid_position(&x, &y);
    _desktop_add_icon(label, x, y);
}

void desktop_delete_icon(int index)
{
    if (index < 0 || index >= _num_icons)
        return;

    /* Shift all _icons after this one down by one */
    for (int i = index; i < _num_icons - 1; i++) {
        _icons[i] = _icons[i + 1];
    }

    /* Clear the last slot */
    memset(&_icons[_num_icons - 1], 0, sizeof(desktop_icon_t));
    _num_icons--;
}

int desktop_get_selected_icon(void)
{
    for (int i = 0; i < _num_icons; i++) {
        if (_icons[i].selected) {
            return i;
        }
    }
    return -1;
}

int desktop_get_icon_at(int x, int y)
{
    for (int i = 0; i < _num_icons; i++) {
        desktop_icon_t *icon = &_icons[i];

        /* Check if point is within icon bounds (including label area) */
        if (x >= icon->x && x < icon->x + ICON_WIDTH && y >= icon->y
            && y < icon->y + ICON_HEIGHT + FONT_HEIGHT + ICON_TEXT_OFFSET) {
            return i;
        }
    }
    return -1;
}

void desktop_clear_context_menu(void)
{
    _context_menu.num_items = 0;
    _context_menu.hover_index = -1;
}

void desktop_add_context_menu_item(const char *label, const char *shortcut, void (*callback)(void))
{
    if (_context_menu.num_items >= MAX_CONTEXT_MENU_ITEMS)
        return;

    context_menu_item_t *item = &_context_menu.items[_context_menu.num_items++];
    strncpy(item->label, label, 31);
    item->label[31] = '\0';
    if (shortcut) {
        strncpy(item->shortcut, shortcut, 15);
        item->shortcut[15] = '\0';
    } else {
        item->shortcut[0] = '\0';
    }
    item->separator = false;
    item->enabled = true;
    item->checked = false;
    item->callback = callback;
}

void desktop_add_context_menu_item_checked(
    const char *label, const char *shortcut, bool checked, void (*callback)(void))
{
    if (_context_menu.num_items >= MAX_CONTEXT_MENU_ITEMS)
        return;

    context_menu_item_t *item = &_context_menu.items[_context_menu.num_items++];
    strncpy(item->label, label, 31);
    item->label[31] = '\0';
    if (shortcut) {
        strncpy(item->shortcut, shortcut, 15);
        item->shortcut[15] = '\0';
    } else {
        item->shortcut[0] = '\0';
    }
    item->separator = false;
    item->enabled = true;
    item->checked = checked;
    item->callback = callback;
}

void desktop_add_context_menu_item_disabled(const char *label, const char *shortcut)
{
    if (_context_menu.num_items >= MAX_CONTEXT_MENU_ITEMS)
        return;

    context_menu_item_t *item = &_context_menu.items[_context_menu.num_items++];
    strncpy(item->label, label, 31);
    item->label[31] = '\0';
    if (shortcut) {
        strncpy(item->shortcut, shortcut, 15);
        item->shortcut[15] = '\0';
    } else {
        item->shortcut[0] = '\0';
    }
    item->separator = false;
    item->enabled = false;
    item->checked = false;
    item->callback = NULL;
}

void desktop_add_context_menu_separator(void)
{
    if (_context_menu.num_items >= MAX_CONTEXT_MENU_ITEMS)
        return;

    context_menu_item_t *item = &_context_menu.items[_context_menu.num_items++];
    item->separator = true;
    item->enabled = false;
    item->checked = false;
    item->callback = NULL;
    item->label[0] = '\0';
    item->shortcut[0] = '\0';
}

void desktop_close_context_menu(void)
{
    _context_menu.visible = false;
    _context_menu.hover_index = -1;
}

bool desktop_context_menu_visible(void)
{
    return _context_menu.visible;
}

void desktop_set_context_menu_builder(context_menu_builder_t builder)
{
    _context_menu_builder = builder;
}

void _desktop_show_context_menu(int x, int y)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Close any open dropdown _menu */
    _menu_open = false;

    /* Clear previous items and let builder populate the _menu */
    desktop_clear_context_menu();

    if (_context_menu_builder) {
        _context_menu_builder();
    }

    /* Don't show empty _menu */
    if (_context_menu.num_items == 0) {
        return;
    }

    /* Calculate _menu dimensions */
    int menu_w, menu_h;
    _get_context_menu_dimensions(&menu_w, &menu_h);

    /* Adjust position to keep _menu on screen */
    if (x + menu_w > (int) fb->width) {
        x = (int) fb->width - menu_w - 2;
    }
    if (y + menu_h > (int) fb->height) {
        y = (int) fb->height - menu_h - 2;
    }
    if (x < 0)
        x = 0;
    if (y < TASKBAR_HEIGHT)
        y = TASKBAR_HEIGHT;

    _context_menu.x = x;
    _context_menu.y = y;
    _context_menu.visible = true;
    _context_menu.hover_index = -1;
}

bool desktop_process_input(void)
{
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool clicked = input_mouse_left_clicked();
    bool right_clicked = input_mouse_right_clicked();
    bool left_down = input_mouse_left();
    framebuffer_t *fb = graphics_get_fb();

    /* Process context _menu input first (it may consume clicks) */
    if (_process_context_menu_input()) {
        return true;
    }

    /* Handle right-click to show context _menu */
    if (right_clicked) {
        /* Close any open dropdown _menu */
        _menu_open = false;

        /* Only show context _menu if not on taskbar and not on a window */
        if (my >= TASKBAR_HEIGHT) {
            window_t *win = wm_window_at_point(mx, my);
            if (win == NULL) {
                /* Check if right-clicking on an icon */
                int icon_idx = desktop_get_icon_at(mx, my);
                if (icon_idx >= 0) {
                    /* Select this icon and deselect others */
                    for (int i = 0; i < _num_icons; i++) {
                        _icons[i].selected = (i == icon_idx);
                    }
                } else {
                    /* Right-click on empty desktop - deselect all _icons */
                    for (int i = 0; i < _num_icons; i++) {
                        _icons[i].selected = false;
                    }
                }

                /* Show context _menu */
                _desktop_show_context_menu(mx, my);
                return true;
            }
        }
    }

    /* Handle taskbar button release */
    if (_taskbar_pressed_window >= 0 && !left_down) {
        /* Button was released - check if still over the same button */
        int sep_x = TASKBAR_START_BUTTON_WIDTH + 6;
        int btn_x = sep_x + 6;
        int num_windows = wm_get_window_count();

        for (int i = 0; i < num_windows; i++) {
            window_t *win = wm_get_window_by_creation_order(i);
            if (!win)
                continue;

            int available_width = (int) fb->width - btn_x - 10;
            int max_buttons = available_width / (TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_MARGIN);
            int btn_width = TASKBAR_BUTTON_WIDTH;
            if (num_windows > max_buttons && max_buttons > 0) {
                btn_width = (available_width / num_windows) - TASKBAR_BUTTON_MARGIN;
                if (btn_width < 40)
                    btn_width = 40;
            }

            rect_t btn = {btn_x, 3, btn_width, TASKBAR_HEIGHT - 6};

            if (win->id == _taskbar_pressed_window && _point_in_rect(mx, my, btn)) {
                /* Released on the same button - perform action */
                if (win->flags & WINDOW_FLAG_MINIMIZED) {
                    wm_restore_window(win);
                } else {
                    wm_bring_to_front(win);
                }
                _menu_open = false;
            }

            btn_x += btn_width + TASKBAR_BUTTON_MARGIN;
        }
        _taskbar_pressed_window = -1;
    }

    /* Check taskbar clicks */
    if (my < TASKBAR_HEIGHT) {
        if (clicked) {
            /* Check if clicked on start/_menu button */
            rect_t start_btn = {2, 3, TASKBAR_START_BUTTON_WIDTH, TASKBAR_HEIGHT - 6};
            if (_point_in_rect(mx, my, start_btn)) {
                if (_menu_open) {
                    _menu_open = false;
                } else {
                    _menu_open = true;
                }
                return true;
            }

            /* Check if clicked on a window button - start press */
            int sep_x = TASKBAR_START_BUTTON_WIDTH + 6;
            int btn_x = sep_x + 6;
            int num_windows = wm_get_window_count();

            for (int i = 0; i < num_windows; i++) {
                window_t *win = wm_get_window_by_creation_order(i);
                if (!win)
                    continue;

                /* Calculate button width */
                int available_width = (int) fb->width - btn_x - 10;
                int max_buttons = available_width / (TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_MARGIN);
                int btn_width = TASKBAR_BUTTON_WIDTH;
                if (num_windows > max_buttons && max_buttons > 0) {
                    btn_width = (available_width / num_windows) - TASKBAR_BUTTON_MARGIN;
                    if (btn_width < 40)
                        btn_width = 40;
                }

                rect_t btn = {btn_x, 3, btn_width, TASKBAR_HEIGHT - 6};

                if (_point_in_rect(mx, my, btn)) {
                    /* Start pressing this button */
                    _taskbar_pressed_window = win->id;
                    _menu_open = false; /* Close any open _menu */
                    return true;
                }

                btn_x += btn_width + TASKBAR_BUTTON_MARGIN;
            }

            /* Close _menu if clicked elsewhere on taskbar */
            _menu_open = false;
            return true; /* Consumed - clicked on taskbar */
        }
    }

    /* Check dropdown _menu clicks */
    if (_menu_open) {
        int dropdown_w = _get_menu_dropdown_width(&_menu);
        int dropdown_h = _menu.num_items * 20 + 4;
        int dropdown_x = 2;
        rect_t dropdown = {dropdown_x, TASKBAR_HEIGHT, dropdown_w, dropdown_h};

        if (clicked) {
            if (_point_in_rect(mx, my, dropdown)) {
                /* Find which item was clicked */
                int item_y = my - TASKBAR_HEIGHT - 2;
                int item_idx = item_y / 20;

                if (item_idx >= 0 && item_idx < _menu.num_items) {
                    menu_item_t *item = &_menu.items[item_idx];
                    if (!item->separator && item->enabled && item->callback) {
                        item->callback();
                    }
                }
                _menu_open = false;
                return true; /* Consumed - clicked on dropdown _menu */
            } else {
                /* Clicked outside _menu */
                _menu_open = false;
                /* Don't return true - let the click pass through to windows */
            }
        }
    }

    /* Handle icon dragging */
    if (_dragging_icon >= 0) {
        if (input_mouse_left()) {
            /* Update icon position while dragging */
            desktop_icon_t *icon = &_icons[_dragging_icon];
            int new_x = mx - _drag_offset_x;
            int new_y = my - _drag_offset_y;

            /* Keep icon on screen */
            framebuffer_t *fb = graphics_get_fb();
            if (new_x < 0)
                new_x = 0;
            if (new_y < TASKBAR_HEIGHT)
                new_y = TASKBAR_HEIGHT;
            if (new_x > (int) fb->width - ICON_WIDTH)
                new_x = (int) fb->width - ICON_WIDTH;
            if (new_y > (int) fb->height - ICON_HEIGHT - FONT_HEIGHT - ICON_TEXT_OFFSET)
                new_y = (int) fb->height - ICON_HEIGHT - FONT_HEIGHT - ICON_TEXT_OFFSET;

            icon->x = new_x;
            icon->y = new_y;
        } else {
            /* Mouse released, snap icon to grid and stop dragging */
            desktop_icon_t *icon = &_icons[_dragging_icon];
            _desktop_snap_to_grid(&icon->x, &icon->y);
            _dragging_icon = -1;
        }
        return true; /* Consumed - dragging icon */
    }

    /* Check desktop icon clicks */
    if (clicked && my >= TASKBAR_HEIGHT && !_menu_open) {
        bool found_icon = false;

        for (int i = 0; i < _num_icons; i++) {
            desktop_icon_t *icon = &_icons[i];
            rect_t icon_rect
                = {icon->x, icon->y, ICON_WIDTH, ICON_HEIGHT + FONT_HEIGHT + ICON_TEXT_OFFSET};

            if (_point_in_rect(mx, my, icon_rect)) {
                /* Deselect others */
                for (int j = 0; j < _num_icons; j++) {
                    _icons[j].selected = false;
                }
                icon->selected = true;
                found_icon = true;

                /* Start dragging this icon */
                _dragging_icon = i;
                _drag_offset_x = mx - icon->x;
                _drag_offset_y = my - icon->y;

                break;
            }
        }

        if (found_icon) {
            return true; /* Consumed - clicked on icon */
        }

        /* Click on empty desktop deselects all */
        if (wm_window_at_point(mx, my) == NULL) {
            for (int i = 0; i < _num_icons; i++) {
                _icons[i].selected = false;
            }
        }
    }

    return false; /* Click not consumed by desktop */
}

void desktop_draw(void)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Draw desktop background */
    rect_t desktop = {0, TASKBAR_HEIGHT, (int) fb->width, (int) fb->height - TASKBAR_HEIGHT};
    graphics_fill_rect(desktop, COLOR_DESKTOP);

    /* Draw icons */
    _draw_icons();

    /* Draw windows */
    wm_draw_all();

    /* Draw taskbar (on top of everything) */
    _draw_taskbar();

    /* Draw dropdown menu if open */
    _draw_dropdown_menu();

    /* Draw context menu if visible (on top of everything) */
    _draw_context_menu();
}

int desktop_get_taskbar_height(void)
{
    return TASKBAR_HEIGHT;
}

bool desktop_menu_open(void)
{
    return _menu_open;
}

void desktop_close_menus(void)
{
    _menu_open = false;
}

void desktop_set_cursor(cursor_type_t cursor)
{
    _current_cursor = cursor;
}

cursor_type_t desktop_get_cursor(void)
{
    return _current_cursor;
}

void desktop_draw_cursor(void)
{
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();

    switch (_current_cursor) {
    case CURSOR_RESIZE_NS:
        _draw_cursor_bitmap(mx, my, _cursor_resize_ns_data, _cursor_resize_ns_mask, 7, 7);
        break;
    case CURSOR_RESIZE_EW:
        _draw_cursor_bitmap(mx, my, _cursor_resize_ew_data, _cursor_resize_ew_mask, 7, 7);
        break;
    case CURSOR_RESIZE_NWSE:
        _draw_cursor_bitmap(mx, my, _cursor_resize_nwse_data, _cursor_resize_nwse_mask, 7, 7);
        break;
    case CURSOR_RESIZE_NESW:
        _draw_cursor_bitmap(mx, my, _cursor_resize_nesw_data, _cursor_resize_nesw_mask, 7, 7);
        break;
    case CURSOR_ARROW:
    default:
        /* Draw cursor with mask (hotspot at top-left) */
        _draw_cursor_bitmap(mx, my, _cursor_data, _cursor_mask, 0, 0);
        break;
    }
}
