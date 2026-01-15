/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./font.h"
#include "./input.h"
#include "./magic.h"
#include "./window.h"

#include <libgfx.h>
#include <libgfx/fonts.h>

gfx_font_t default_font;

static void _draw_about_content(gfx_context_t *context, window_t *window, gfx_rect_t area, void *user)
{
    (void) window;
    (void) user;

    gfx_draw_text(
        context,
        &default_font,
        (gfx_pos_t) {.x = area.x + 16, .y = area.y + 32},
        FONT_COLOR,
        "MONOLITH Project");
    gfx_draw_text(
        context,
        &default_font,
        (gfx_pos_t) {.x = area.x + 16, .y = area.y + 64},
        FONT_COLOR,
        "A hobby operating system");
}

static void _menu_action_about(void)
{
    window_t *about = new_window("About MONOLITH", 220, 130);
    window_set_draw_callback(about, _draw_about_content, NULL);
}

static void _menu_action_shutdown(void) {}

static void _menu_action_reboot(void) {}

int main()
{
    gfx_context_t context = gfx_init_screen();
    default_font = gfx_load_polyspace_font(
        font_atlas,
        (gfx_font_size_t) {FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT},
        font_glyphs,
        FONT_FIRST_CHAR,
        FONT_LAST_CHAR,
        (gfx_font_size_t) {13, FONT_LINE_HEIGHT});

    const menu_item_t system_menu_items[] = {
        {.label = "About MONOLITH", .type = MENU_ITEM_ACTION, .action = _menu_action_about},
        {.label = NULL, .type = MENU_ITEM_SEPARATOR, .action = NULL},
        {.label = "Shutdown", .type = MENU_ITEM_ACTION, .action = _menu_action_shutdown},
        {.label = "Reboot", .type = MENU_ITEM_ACTION, .action = _menu_action_reboot},
    };
    menu_t system_menu = {
        .x = 0,
        .y = TOP_BAR_HEIGHT - 1,
        .items = system_menu_items,
        .item_count = sizeof(system_menu_items) / sizeof(system_menu_items[0]),
        .open = false,
    };
    menubar_item_t menubar_items[] = {
        {.label = "System", .menu = &system_menu},
        {.label = "Apps", .menu = NULL},
        {.label = "File", .menu = NULL},
    };
    menubar_t system_menubar = {
        .items = menubar_items,
        .item_count = sizeof(menubar_items) / sizeof(menubar_items[0]),
    };
    set_default_menubar(&system_menubar);
    _menu_action_about();

    while (1) {
        while (!handle_input(&context))
            usleep(1);

        update_menubar_state(&context);
        update_windows_state(&context);

        gfx_clear(&context, (gfx_color_t) {0xff, 0x18, 0x18, 0x18});
        draw_all_windows(&context);
        draw_menubar(&context);
        draw_cursor(&context);
        gfx_flush(&context);
        usleep(1);
    }
}
