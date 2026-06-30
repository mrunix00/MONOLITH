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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

#define TERMINAL_TAP_COUNT 16
#define TERMINAL_TAP_BUFFER_SIZE (sizeof(term_command_t) + TERM_MAX_PAYLOAD)

gfx_font_t default_font;

typedef enum {
    TERMINAL_TAP_COMMAND,
    TERMINAL_TAP_EVENT,
} terminal_tap_kind_t;

typedef struct
{
    bool active;
    bool line_start;
    rsrc_handle_t handle;
    terminal_tap_kind_t kind;
    const char *source;
    uint8_t buffer[TERMINAL_TAP_BUFFER_SIZE];
    uint32_t length;
} terminal_tap_t;

static terminal_tap_t _terminal_taps[TERMINAL_TAP_COUNT];

static terminal_tap_t *_terminal_tap_alloc(
    rsrc_handle_t handle, terminal_tap_kind_t kind, const char *source)
{
    for (uint32_t i = 0; i < TERMINAL_TAP_COUNT; i++) {
        if (_terminal_taps[i].active)
            continue;
        _terminal_taps[i] = (terminal_tap_t){
            .active = true,
            .line_start = true,
            .handle = handle,
            .kind = kind,
            .source = source,
        };
        return &_terminal_taps[i];
    }

    return NULL;
}

static void _terminal_tap_release(rsrc_handle_t handle)
{
    for (uint32_t i = 0; i < TERMINAL_TAP_COUNT; i++) {
        if (_terminal_taps[i].active && _terminal_taps[i].handle == handle)
            _terminal_taps[i] = (terminal_tap_t){0};
    }
}

static void _debug_write_text(const char *text)
{
    static rsrc_handle_t debug_handle = RSRC_INVALID_HANDLE;
    if (!text)
        return;
    if (debug_handle < 0)
        debug_handle = rsmgr_open("device:/debug");
    if (debug_handle >= 0)
        rsmgr_write(debug_handle, text, (uint32_t) strlen(text));
}

static void _debug_write_bytes(const void *bytes, uint32_t length)
{
    static rsrc_handle_t debug_handle = RSRC_INVALID_HANDLE;
    if (!bytes || length == 0)
        return;
    if (debug_handle < 0)
        debug_handle = rsmgr_open("device:/debug");
    if (debug_handle >= 0)
        rsmgr_write(debug_handle, bytes, length);
}

static void _terminal_tap_write_source(terminal_tap_t *tap)
{
    const char *source = tap->source ? tap->source : "unknown";
    _debug_write_bytes(source, (uint32_t) strlen(source));
    _debug_write_bytes(": ", 2);
}

static void _terminal_tap_write_payload(terminal_tap_t *tap, const uint8_t *payload, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        if (tap->line_start) {
            _terminal_tap_write_source(tap);
            tap->line_start = false;
        }

        _debug_write_bytes(&payload[i], 1);
        if (payload[i] == '\n')
            tap->line_start = true;
    }
}

static void _terminal_tap_parse(terminal_tap_t *tap)
{
    uint32_t header_size = tap->kind == TERMINAL_TAP_COMMAND ? sizeof(term_command_t)
                                                             : sizeof(term_event_t);

    while (tap->length >= header_size) {
        uint32_t payload_length = 0;

        if (tap->kind == TERMINAL_TAP_COMMAND) {
            term_command_t command;
            memcpy(&command, tap->buffer, sizeof(command));
            payload_length = command.length;
        } else {
            term_event_t event;
            memcpy(&event, tap->buffer, sizeof(event));
            payload_length = event.length;
        }

        if (payload_length > TERM_MAX_PAYLOAD) {
            tap->length = 0;
            _debug_write_text("terminal tap: invalid packet length\n");
            return;
        }

        uint32_t packet_size = header_size + payload_length;
        if (tap->length < packet_size)
            return;

        _terminal_tap_write_payload(tap, tap->buffer + header_size, payload_length);
        tap->length -= packet_size;
        if (tap->length > 0)
            memmove(tap->buffer, tap->buffer + packet_size, tap->length);
    }
}

static void _terminal_tap_pump_one(terminal_tap_t *tap)
{
    while (tap->active && tap->length < sizeof(tap->buffer)) {
        uint64_t bytes_read = 0;
        int result = rsmgr_read(
            tap->handle,
            tap->buffer + tap->length,
            (uint32_t) (sizeof(tap->buffer) - tap->length),
            &bytes_read);
        if (result < 0 || bytes_read == 0)
            break;
        tap->length += (uint32_t) bytes_read;
        _terminal_tap_parse(tap);
    }
}

static void _terminal_taps_pump(void)
{
    for (uint32_t i = 0; i < TERMINAL_TAP_COUNT; i++) {
        if (_terminal_taps[i].active)
            _terminal_tap_pump_one(&_terminal_taps[i]);
    }
}

static rsrc_handle_t _launch_task(int argc, const char **argv)
{
    rsrc_handle_t tin = pipe_create(NULL);
    rsrc_handle_t tout = pipe_create(NULL);
    if (tin < 0 || tout < 0) {
        debug_log("desktop: failed to create task log pipes\n");
        if (tin >= 0)
            close(tin);
        if (tout >= 0)
            close(tout);
        return RSRC_INVALID_HANDLE;
    }

    task_create_inherit_t inherit[] = {
        {.current_descriptor = tin, .target_descriptor = TERM_RD_TIN},
        {.current_descriptor = tout, .target_descriptor = TERM_RD_TOUT},
    };
    rsrc_handle_t task = task_create(argc, argv, inherit, 2);
    if (task < 0) {
        debug_log("desktop: failed to launch logged task\n");
        close(tin);
        close(tout);
        return RSRC_INVALID_HANDLE;
    }

    if (!_terminal_tap_alloc(tin, TERMINAL_TAP_COMMAND, argv[0])
        || !_terminal_tap_alloc(tout, TERMINAL_TAP_EVENT, argv[0])) {
        debug_log("desktop: failed to register task log pipes\n");
        _terminal_tap_release(tin);
        _terminal_tap_release(tout);
        close(tin);
        close(tout);
    }

    return task;
}

static void _menu_action_about(void)
{
    _launch_task(1, (const char *[]){"file:/system/about"});
}

static void _menu_action_gfxdemo(void)
{
    _launch_task(1, (const char *[]){"file:/system/gfxdemo"});
}

static void _menu_action_doom(void)
{
    _launch_task(2, (const char *[]){"file:/system/doom", "file:/system/assets/doom1.wad"});
}

static void _menu_action_terminal(void)
{
    _launch_task(1, (const char *[]){"file:/system/terminal"});
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

        uint64_t bytes_read = 0;
        int result = rsmgr_read(fd, img_data + total_read, 8192, &bytes_read);
        if (result < 0 || bytes_read == 0)
            break;
        total_read += bytes_read;
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
        {.label = "Terminal", .type = MENU_ITEM_ACTION, .action = _menu_action_terminal},
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

    bool launched_initial_term = false;
    bool needs_redraw = true;

    while (1) {
        window_set_screen_bounds((uint32_t) context.width, (uint32_t) context.height);
        input_set_screen_bounds((uint32_t) context.width, (uint32_t) context.height);

        bool client_activity = protocol_server_pump();
        bool menubar_changed = update_menubar_state(&context);
        bool windows_changed = update_windows_state(&context);
        _terminal_taps_pump();
        if (!launched_initial_term) {
            _menu_action_terminal();
            launched_initial_term = true;
            client_activity = true;
        }

        needs_redraw = needs_redraw || client_activity || menubar_changed || windows_changed;

        if (!needs_redraw) {
            protocol_server_wait();
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
        gfx_wait_frame(&context);
        needs_redraw = false;
    }
}
