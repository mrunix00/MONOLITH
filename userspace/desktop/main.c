/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./input.h"
#include "./magic.h"
#include "./protocol_server.h"
#include "./window.h"

#include <debug.h>
#include <libgfx.h>
#include <libgfx/images.h>
#include <resource.h>
#include <stdlib.h>
#include <unistd.h>

gfx_font_t default_font;

static void _menu_action_about(void)
{
    spawn(1, (const char *[]) {"file:/system/about"});
}

static void _menu_action_gfxdemo(void)
{
    spawn(1, (const char *[]) {"file:/system/gfxdemo"});
}

static void _menu_action_doom(void)
{
    spawn(2, (const char *[]) {"file:/system/doom", "file:/system/assets/doom1.wad"});
}

static gfx_colored_bitmap_t _load_wallpaper(const char *wallpaper, uint32_t width, uint32_t height)
{
    int fd = rsmgr_open(wallpaper);
    if (fd < 0)
        return (gfx_colored_bitmap_t){0};

    uint8_t *img_data = NULL;
    uint64_t total_read = 0;
    uint32_t capacity = 0;

    while (1) {
        if (total_read + 8192 > capacity) {
            uint32_t new_cap = capacity == 0 ? 16384 : capacity * 2;
            uint8_t *new_buf = realloc(img_data, new_cap);
            if (new_buf == NULL) {
                free(img_data);
                rsmgr_close(fd);
                return (gfx_colored_bitmap_t){0};
            }
            img_data = new_buf;
            capacity = new_cap;
        }

        int bytes_read = rsmgr_read(fd, img_data + total_read, 8192);
        if (bytes_read <= 0)
            break;
        total_read += (uint64_t) bytes_read;
    }

    rsmgr_close(fd);

    if (total_read == 0) {
        free(img_data);
        return (gfx_colored_bitmap_t){0};
    }

    gfx_colored_bitmap_t wallpaper_img = gfx_load_image(img_data, (size_t) total_read);
    free(img_data);
    gfx_resize_image(&wallpaper_img, width, height);

    return wallpaper_img;
}

int main()
{
    if (protocol_server_init() != 0) {
        debug_log("failed to initialize protocol server\n");
        return 1;
    }

    gfx_context_t context = gfx_init_screen();
    if (FRAME_RATE > 0)
        gfx_set_target_fps(&context, FRAME_RATE);
    default_font
        = gfx_load_font_from_file("file:/system/assets/IBMPlexSans_Condensed-Medium.ttf", 18);

    gfx_colored_bitmap_t wallpaper
        = _load_wallpaper("file:/system/assets/wallpaper.jpg", context.width, context.height);

    const menu_item_t system_menu_items[] = {
        {.label = "About MONOLITH", .type = MENU_ITEM_ACTION, .action = _menu_action_about},
    };
    const menu_item_t apps_items[] = {
        {.label = "GFX Demo", .type = MENU_ITEM_ACTION, .action = _menu_action_gfxdemo},
        {.label = "DOOM", .type = MENU_ITEM_ACTION, .action = _menu_action_doom},
    };
    menu_t system_menu = {
        .items = system_menu_items,
        .item_count = sizeof(system_menu_items) / sizeof(system_menu_items[0]),
        .open = false,
    };
    menu_t apps_menu = {
        .items = apps_items,
        .item_count = sizeof(apps_items) / sizeof(apps_items[0]),
        .open = false,
    };
    menubar_item_t menubar_items[] = {
        {.label = "System", .menu = &system_menu},
        {.label = "Apps", .menu = &apps_menu},
    };
    menubar_t system_menubar = {
        .items = menubar_items,
        .item_count = sizeof(menubar_items) / sizeof(menubar_items[0]),
    };
    set_default_menubar(&system_menubar);

    bool launched_initial_about = false;
    bool needs_redraw = true;

    while (1) {
        window_set_screen_bounds((uint32_t) context.width, (uint32_t) context.height);
        input_set_screen_bounds((uint32_t) context.width, (uint32_t) context.height);

        bool client_activity = protocol_server_pump();
        bool menubar_changed = update_menubar_state(&context);
        bool windows_changed = update_windows_state(&context);
        if (!launched_initial_about) {
            _menu_action_about();
            launched_initial_about = true;
            client_activity = true;
        }

        needs_redraw = needs_redraw || client_activity || menubar_changed || windows_changed;

        if (!needs_redraw) {
            usleep(1);
            continue;
        }

        gfx_begin_frame(&context);

        if (wallpaper.data)
            gfx_draw_colored_bitmap(&context, &wallpaper, (gfx_pos_t){0, 0});
        else
            gfx_clear(&context, (gfx_color_t){0xff, 0x18, 0x18, 0x18});

        draw_all_windows(&context);
        draw_menubar(&context);
        draw_cursor(&context);
        gfx_draw_fps_counter(
            &context,
            &default_font,
            (gfx_color_t){0xff, 0xff, 0xff, 0xff},
            (gfx_pos_t){context.width - 60, context.height - 10});

        gfx_end_frame(&context);
        needs_redraw = false;
    }
}
