/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <debug.h>
#include <libdesktop.h>
#include <libgfx.h>
#include <unistd.h>

static void _draw_about_view(uint16_t width, uint16_t height, gfx_font_t *font, gfx_context_t *ctx)
{
    static const char *title_text = "MONOLITH Project";
    static const char *subtitle_text = "A hobby operating system";

    uint16_t draw_width = width;
    uint16_t draw_height = height;
    if (draw_width > ctx->width)
        draw_width = (uint16_t) ctx->width;
    if (draw_height > ctx->height)
        draw_height = (uint16_t) ctx->height;

    if (draw_width < 64 || draw_height < 64)
        return;

    gfx_begin_frame(ctx);

    gfx_draw_filled_rect(
        ctx,
        (gfx_rect_t){0, 0, draw_width, draw_height, {0}, 0},
        (gfx_color_t){0xFF, 0x1F, 0x22, 0x28});

    gfx_set_clip(ctx, (gfx_area_t){0, 0, draw_width, draw_height});

    uint32_t title_width = gfx_get_text_width(font, title_text);
    uint32_t subtitle_width = gfx_get_text_width(font, subtitle_text);
    uint32_t block_width = title_width > subtitle_width ? title_width : subtitle_width;
    uint32_t line_height = gfx_get_text_height(font, title_text);
    uint32_t line_gap = line_height / 2;
    uint32_t block_height = line_height * 2 + line_gap;

    uint32_t block_x = draw_width > block_width ? (draw_width - block_width) / 2 : 0;
    uint32_t block_y = draw_height > block_height ? (draw_height - block_height) / 2 : 0;

    uint32_t title_x = block_x + (block_width > title_width ? (block_width - title_width) / 2 : 0);
    uint32_t subtitle_x = block_x
                          + (block_width > subtitle_width ? (block_width - subtitle_width) / 2 : 0);
    uint32_t title_y = block_y;
    uint32_t subtitle_y = block_y + line_height + line_gap;

    gfx_draw_text(
        ctx,
        font,
        (gfx_pos_t){.x = title_x, .y = title_y},
        (gfx_color_t){0xFF, 0xDF, 0xE4, 0xED},
        title_text);
    gfx_draw_text(
        ctx,
        font,
        (gfx_pos_t){.x = subtitle_x, .y = subtitle_y},
        (gfx_color_t){0xFF, 0xB9, 0xC2, 0xCF},
        subtitle_text);

    gfx_end_frame(ctx);
}

int main()
{
    bool created = false;
    bool create_pending = false;
    uint32_t create_sequence = 0;
    gfx_context_t framebuffer = {0};
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t window_id = 0;
    gfx_font_t _about_font
        = gfx_load_font_from_file("file:/system/assets/IBMPlexSans_Condensed-Medium.ttf", 18);

    if (_about_font.data == NULL) {
        debug_log("failed to load font\n");
        return 1;
    }
    if (desktop_connect() < 0) {
        debug_log("failed to connect to desktop\n");
        return 1;
    }

    while (1) {
        if (!created) {
            if (!create_pending) {
                create_sequence = desktop_create_window(
                    360,
                    190,
                    (window_flags_t){
                        .borderless = false,
                        .fullscreen = false,
                        .resizable = true,
                        .maximized = false,
                        .minimized = false,
                    },
                    "About MONOLITH");
                create_pending = true;
            }
        }

        desktop_event_t event;
        if (desktop_wait_event(&event) != 0)
            continue;

        if (event.type == DESKTOP_EVENT_WINDOW_CREATED && create_pending
            && event.sequence == create_sequence) {
            if (event.data.created.status != WINDOW_CREATED_SUCCESS) {
                create_pending = false;
                continue;
            }

            created = true;
            create_pending = false;
            width = event.data.created.width;
            height = event.data.created.height;
            window_id = event.data.created.window_id;
            desktop_request_window_framebuffer(window_id, width, height);
            continue;
        }

        if (event.type == DESKTOP_EVENT_FRAMEBUFFER_READY
            && event.data.framebuffer_ready.id == window_id
            && desktop_handle_framebuffer_event(&event, &framebuffer) == 1) {
            _draw_about_view(width, height, &_about_font, &framebuffer);
            desktop_present_window(window_id);
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_RESIZED
            && event.data.resized.window_id == window_id) {
            width = event.data.resized.new_width;
            height = event.data.resized.new_height;
            if (desktop_request_window_framebuffer(window_id, width, height) == 1) {
                _draw_about_view(width, height, &_about_font, &framebuffer);
                desktop_present_window(window_id);
            }
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_CLOSE && event.data.close.window_id == window_id) {
            desktop_destroy_window((uint16_t) window_id);
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_CLOSED && event.data.closed.window_id == window_id)
            break;
    }

    return 0;
}
