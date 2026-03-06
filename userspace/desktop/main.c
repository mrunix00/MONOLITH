/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./font.h"
#include "./input.h"
#include "./magic.h"
#include "./protocol_server.h"
#include "./window.h"

#include <libgfx.h>
#include <libgfx/images.h>
#include <stdlib.h>

gfx_font_t default_font;

static void _menu_action_about(void)
{
    syscall1(SYSCALL_SPAWN_TASK, (long) "system:/about");
}

static void _menu_action_gfxdemo(void)
{
    syscall1(SYSCALL_SPAWN_TASK, (long) "system:/gfxdemo");
}

static void _menu_action_shutdown(void) {}

static void _menu_action_reboot(void) {}

static gfx_colored_bitmap_t _load_wallpaper(const char *wallpaper, uint32_t width, uint32_t height)
{
    int fd = open(wallpaper, O_RDONLY);
    if (fd < 0)
        return (gfx_colored_bitmap_t) {0};

    file_stats_t stats;
    if (fstat(fd, &stats) < 0) {
        close(fd);
        return (gfx_colored_bitmap_t) {0};
    }
    if (stats.type != TYPE_FILE || stats.size == 0) {
        close(fd);
        return (gfx_colored_bitmap_t) {0};
    }

    uint8_t *img_data = malloc((size_t) stats.size);
    if (img_data == NULL) {
        close(fd);
        return (gfx_colored_bitmap_t) {0};
    }

    uint64_t read_size = 0;
    do {
        uint32_t chunk = (uint32_t) ((stats.size - read_size) > 8192 ? 8192
                                                                     : (stats.size - read_size));
        int bytes_read = read(fd, img_data + read_size, chunk);
        if (bytes_read <= 0) {
            close(fd);
            free(img_data);
            return (gfx_colored_bitmap_t) {0};
        }
        read_size += (uint64_t) bytes_read;
    } while (read_size < stats.size);

    close(fd);

    gfx_colored_bitmap_t wallpaper_img = gfx_load_image(img_data, (size_t) stats.size);
    free(img_data);
    gfx_resize_image(&wallpaper_img, width, height);

    return wallpaper_img;
}

int main()
{
    if (protocol_server_init() != 0)
        return 1;

    gfx_context_t context = gfx_init_screen();
    if (FRAME_RATE > 0)
        gfx_set_target_fps(&context, FRAME_RATE);
    default_font = gfx_load_polyspace_font(
        font_atlas,
        (gfx_font_size_t) {FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT},
        font_glyphs,
        FONT_FIRST_CHAR,
        FONT_LAST_CHAR,
        (gfx_font_size_t) {13, FONT_LINE_HEIGHT});

    gfx_colored_bitmap_t wallpaper
        = _load_wallpaper("system://assets/wallpaper.jpg", context.width, context.height);

    const menu_item_t system_menu_items[] = {
        {.label = "About MONOLITH", .type = MENU_ITEM_ACTION, .action = _menu_action_about},
        {.label = "GFX Demo", .type = MENU_ITEM_ACTION, .action = _menu_action_gfxdemo},
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
            gfx_draw_colored_bitmap(&context, &wallpaper, (gfx_pos_t) {0, 0});
        else
            gfx_clear(&context, (gfx_color_t) {0xff, 0x18, 0x18, 0x18});

        draw_all_windows(&context);
        draw_menubar(&context);
        draw_cursor(&context);
        gfx_draw_fps_counter(
            &context,
            &default_font,
            (gfx_color_t) {0xff, 0xff, 0xff, 0xff},
            (gfx_pos_t) {context.width - 60, context.height - 10});

        gfx_end_frame(&context);
        needs_redraw = false;
    }
}
