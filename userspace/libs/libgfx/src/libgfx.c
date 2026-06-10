/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define abs(x) ((x) < 0 ? -(x) : (x))
#define pack_color(c) ((c.a << 24) | (c.r << 16) | (c.g << 8) | c.b)

static inline uint32_t _scale_to_mask(uint8_t v, uint8_t size)
{
    if (size == 0)
        return 0;
    if (size >= 8)
        return (uint32_t) v;
    uint32_t max = (1u << size) - 1u;
    return (uint32_t) ((v * max + 127) / 255);
}

static inline uint8_t _fb_bytes_per_pixel(const gfx_context_t *ctx)
{
    return (uint8_t) ((ctx->bpp + 7) / 8);
}

static inline int _fb_format_matches_argb(const gfx_context_t *ctx)
{
    return ctx->bpp == 32 && ctx->red_mask_size == 8 && ctx->green_mask_size == 8
           && ctx->blue_mask_size == 8 && ctx->red_mask_shift == 16 && ctx->green_mask_shift == 8
           && ctx->blue_mask_shift == 0;
}

static inline uint32_t _fb_pack_rgb(const gfx_context_t *ctx, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pr = _scale_to_mask(r, ctx->red_mask_size);
    uint32_t pg = _scale_to_mask(g, ctx->green_mask_size);
    uint32_t pb = _scale_to_mask(b, ctx->blue_mask_size);
    return (pr << ctx->red_mask_shift) | (pg << ctx->green_mask_shift)
           | (pb << ctx->blue_mask_shift);
}

static inline void _blend_pixel(uint32_t *dst_ptr, uint32_t src)
{
    uint8_t _a = (uint8_t) (src >> 24);
    if (_a == 0)
        return;
    if (_a == 255) {
        *(dst_ptr) = src;
        return;
    }
    uint32_t _dst = *(dst_ptr);
    uint8_t _dst_a = (uint8_t) (_dst >> 24);
    uint8_t _dst_r = (uint8_t) (_dst >> 16);
    uint8_t _dst_g = (uint8_t) (_dst >> 8);
    uint8_t _dst_b = (uint8_t) _dst;
    uint32_t _inv_a = 255 - _a;
    uint8_t _out_a = (uint8_t) ((_a + (_dst_a * _inv_a + 127) / 255) & 0xFF);
    uint8_t _out_r = (uint8_t) ((((src >> 16) & 0xFF) * _a + _dst_r * _inv_a + 127) / 255);
    uint8_t _out_g = (uint8_t) ((((src >> 8) & 0xFF) * _a + _dst_g * _inv_a + 127) / 255);
    uint8_t _out_b = (uint8_t) (((src & 0xFF) * _a + _dst_b * _inv_a + 127) / 255);
    *(dst_ptr) = ((uint32_t) _out_a << 24) | ((uint32_t) _out_r << 16) | ((uint32_t) _out_g << 8)
                 | (uint32_t) _out_b;
}

typedef struct
{
    uint32_t *framebuffer;
    size_t width;
    size_t height;
    size_t pitch;
    size_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} _framebuffer_t;

gfx_context_t gfx_init_screen()
{
    _framebuffer_t fb;
    syscall1(SYSCALL_REQUEST_FB, (long) &fb);

    uint16_t bpp = fb.bpp ? fb.bpp : 32;
    uint8_t memory_model = fb.memory_model ? fb.memory_model : 1;

    uint8_t red_mask_size = fb.red_mask_size;
    uint8_t red_mask_shift = fb.red_mask_shift;
    uint8_t green_mask_size = fb.green_mask_size;
    uint8_t green_mask_shift = fb.green_mask_shift;
    uint8_t blue_mask_size = fb.blue_mask_size;
    uint8_t blue_mask_shift = fb.blue_mask_shift;

    if (red_mask_size == 0 && green_mask_size == 0 && blue_mask_size == 0) {
        red_mask_size = 8;
        red_mask_shift = 16;
        green_mask_size = 8;
        green_mask_shift = 8;
        blue_mask_size = 8;
        blue_mask_shift = 0;
    }

    uint64_t pitch = fb.pitch ? fb.pitch : (uint64_t) fb.width * ((bpp + 7) / 8);

    gfx_context_t ctx = {
        .framebuffer = fb.framebuffer,
        .width = fb.width,
        .height = fb.height,
        .pitch = pitch,
        .bpp = bpp,
        .memory_model = memory_model,
        .red_mask_size = red_mask_size,
        .red_mask_shift = red_mask_shift,
        .green_mask_size = green_mask_size,
        .green_mask_shift = green_mask_shift,
        .blue_mask_size = blue_mask_size,
        .blue_mask_shift = blue_mask_shift,
        .target_fps = 0,
        .frame_start_ticks = 0,
        .last_frame_ticks = 0,
        .fps_last_update_ticks = get_ticks(),
        .fps_frame_count = 0,
        .fps_last_value = 0,
        .backbuffer = malloc(fb.width * fb.height * sizeof(uint32_t)),
        .clip_rect = {0, 0, (uint32_t) fb.width, (uint32_t) fb.height},
    };
    return ctx;
}

void gfx_deinit(gfx_context_t *context)
{
    free(context->backbuffer);
}

static void _gfx_update_fps(gfx_context_t *ctx, uint64_t now)
{
    ctx->fps_frame_count++;

    if (ctx->fps_last_update_ticks == 0) {
        ctx->fps_last_update_ticks = now;
        return;
    }

    uint64_t elapsed = now - ctx->fps_last_update_ticks;
    if (elapsed >= 1000) {
        ctx->fps_last_value = (uint32_t) ((ctx->fps_frame_count * 1000) / elapsed);
        ctx->fps_frame_count = 0;
        ctx->fps_last_update_ticks = now;
    }
}

void gfx_set_target_fps(gfx_context_t *ctx, uint32_t fps)
{
    if (!ctx)
        return;
    ctx->target_fps = fps;
}

void gfx_begin_frame(gfx_context_t *ctx)
{
    if (!ctx)
        return;
    ctx->frame_start_ticks = get_ticks();
}

void gfx_end_frame(gfx_context_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->frame_start_ticks == 0)
        return;

    if (!ctx->framebuffer || !ctx->backbuffer)
        return;

    uint8_t *dst = (uint8_t *) ctx->framebuffer;
    size_t width = ctx->width;
    size_t height = ctx->height;
    uint8_t bpp_bytes = _fb_bytes_per_pixel(ctx);

    if (ctx->memory_model == 1 && _fb_format_matches_argb(ctx) && bpp_bytes == 4) {
        size_t row_bytes = width * sizeof(uint32_t);
        if (ctx->pitch == row_bytes) {
            memcpy(ctx->framebuffer, ctx->backbuffer, row_bytes * height);
        } else {
            for (size_t y = 0; y < height; y++)
                memcpy(dst + y * ctx->pitch, ctx->backbuffer + y * width, row_bytes);
        }
    } else {
        for (size_t y = 0; y < height; y++) {
            uint8_t *dst_row = dst + y * ctx->pitch;
            const uint32_t *src_row = ctx->backbuffer + y * width;

            for (size_t x = 0; x < width; x++) {
                uint32_t src = src_row[x];
                uint8_t r = (uint8_t) (src >> 16);
                uint8_t g = (uint8_t) (src >> 8);
                uint8_t b = (uint8_t) src;

                uint32_t packed = (ctx->memory_model == 1) ? _fb_pack_rgb(ctx, r, g, b) : src;
                uint8_t *p = dst_row + x * bpp_bytes;

                if (bpp_bytes == 4) {
                    *(uint32_t *) p = packed;
                } else {
                    for (uint8_t i = 0; i < bpp_bytes; i++) {
                        p[i] = (uint8_t) (packed >> (i * 8));
                    }
                }
            }
        }
    }

    uint64_t now = get_ticks();
    ctx->last_frame_ticks = now - ctx->frame_start_ticks;

    if (ctx->target_fps != 0) {
        uint64_t frame_budget = 1000 / ctx->target_fps;
        if (ctx->last_frame_ticks < frame_budget)
            usleep((unsigned int) (frame_budget - ctx->last_frame_ticks));
    }

    now = get_ticks();
    ctx->last_frame_ticks = now - ctx->frame_start_ticks;
    _gfx_update_fps(ctx, now);
}

void gfx_draw_fps_counter(gfx_context_t *ctx, gfx_font_t *font, gfx_color_t color, gfx_pos_t pos)
{
    char text[24];
    sprintf(text, "FPS: %u", (unsigned int) ctx->fps_last_value);
    gfx_draw_text(ctx, font, pos, color, text);
}

void gfx_clear(gfx_context_t *ctx, gfx_color_t color)
{
    uint32_t packed = pack_color(color);

    uint32_t x1 = ctx->clip_rect.x;
    uint32_t y1 = ctx->clip_rect.y;
    uint32_t x2 = ctx->clip_rect.x + ctx->clip_rect.width;
    uint32_t y2 = ctx->clip_rect.y + ctx->clip_rect.height;

    if (x1 > ctx->width)
        x1 = (uint32_t) ctx->width;
    if (y1 > ctx->height)
        y1 = (uint32_t) ctx->height;
    if (x2 > ctx->width)
        x2 = (uint32_t) ctx->width;
    if (y2 > ctx->height)
        y2 = (uint32_t) ctx->height;

    if (x1 >= x2 || y1 >= y2)
        return;

    uint32_t draw_width = x2 - x1;
    for (uint32_t y = y1; y < y2; y++) {
        uint32_t *dst_row = ctx->backbuffer + (size_t) y * ctx->width + x1;
        for (uint32_t x = 0; x < draw_width; x++)
            dst_row[x] = packed;
    }
}

static inline int point_in_clip(gfx_context_t *ctx, uint32_t x, uint32_t y)
{
    return x >= ctx->clip_rect.x && x < ctx->clip_rect.x + ctx->clip_rect.width
           && y >= ctx->clip_rect.y && y < ctx->clip_rect.y + ctx->clip_rect.height;
}

void gfx_draw_pixel(gfx_context_t *ctx, gfx_pos_t point, gfx_color_t color)
{
    if (point.x < ctx->width && point.y < ctx->height && point_in_clip(ctx, point.x, point.y)) {
        size_t idx = (size_t) point.y * ctx->width + (size_t) point.x;
        _blend_pixel(&ctx->backbuffer[idx], pack_color(color));
    }
}

void gfx_set_clip(gfx_context_t *ctx, gfx_area_t rect)
{
    ctx->clip_rect = rect;

    /* Clamp to screen */
    if (ctx->clip_rect.x > ctx->width) {
        ctx->clip_rect.x = (uint32_t) ctx->width;
        ctx->clip_rect.width = 0;
    } else if (ctx->clip_rect.x + ctx->clip_rect.width > ctx->width) {
        ctx->clip_rect.width = (uint32_t) ctx->width - ctx->clip_rect.x;
    }
    if (ctx->clip_rect.y > ctx->height) {
        ctx->clip_rect.y = (uint32_t) ctx->height;
        ctx->clip_rect.height = 0;
    } else if (ctx->clip_rect.y + ctx->clip_rect.height > ctx->height) {
        ctx->clip_rect.height = (uint32_t) ctx->height - ctx->clip_rect.y;
    }
}

void gfx_reset_clip(gfx_context_t *ctx)
{
    ctx->clip_rect.x = 0;
    ctx->clip_rect.y = 0;
    ctx->clip_rect.width = (uint32_t) ctx->width;
    ctx->clip_rect.height = (uint32_t) ctx->height;
}

void gfx_draw_rect(gfx_context_t *ctx, gfx_rect_t rect)
{
    /* Top and bottom lines */
    gfx_draw_line(
        ctx,
        (gfx_line_t){
            rect.x,
            rect.y,
            rect.x + rect.width - 1,
            rect.y,
            rect.border_thickness,
        },
        rect.border_color);
    gfx_draw_line(
        ctx,
        (gfx_line_t){
            rect.x,
            rect.y + rect.height - 1,
            rect.x + rect.width - 1,
            rect.y + rect.height - 1,
            rect.border_thickness,
        },
        rect.border_color);

    /* Left and right lines */
    gfx_draw_line(
        ctx,
        (gfx_line_t){
            rect.x,
            rect.y + rect.border_thickness,
            rect.x,
            rect.y + rect.height - 1 - rect.border_thickness,
            rect.border_thickness,
        },
        rect.border_color);
    gfx_draw_line(
        ctx,
        (gfx_line_t){
            rect.x + rect.width - 1,
            rect.y + rect.border_thickness,
            rect.x + rect.width - 1,
            rect.y + rect.height - 1 - rect.border_thickness,
            rect.border_thickness,
        },
        rect.border_color);
}

void gfx_draw_filled_rect(gfx_context_t *ctx, gfx_rect_t rect, gfx_color_t fill)
{
    uint32_t x1 = rect.x;
    uint32_t y1 = rect.y;
    uint32_t x2 = rect.x + rect.width;
    uint32_t y2 = rect.y + rect.height;

    /* Clip */
    if (x1 < ctx->clip_rect.x)
        x1 = ctx->clip_rect.x;
    if (y1 < ctx->clip_rect.y)
        y1 = ctx->clip_rect.y;
    if (x2 > ctx->clip_rect.x + ctx->clip_rect.width)
        x2 = ctx->clip_rect.x + ctx->clip_rect.width;
    if (y2 > ctx->clip_rect.y + ctx->clip_rect.height)
        y2 = ctx->clip_rect.y + ctx->clip_rect.height;

    /* Clamp to screen */
    if (x2 > ctx->width)
        x2 = (uint32_t) ctx->width;
    if (y2 > ctx->height)
        y2 = (uint32_t) ctx->height;

    if (x1 >= x2 || y1 >= y2)
        return;

    if (fill.a != 0) {
        uint32_t packed_fill = pack_color(fill);
        uint32_t draw_width = x2 - x1;

        for (uint32_t y = y1; y < y2; y++) {
            uint32_t *dst_row = ctx->backbuffer + (size_t) y * ctx->width + x1;
            for (uint32_t x = 0; x < draw_width; x++)
                _blend_pixel(&dst_row[x], packed_fill);
        }
    }

    if (rect.border_thickness > 0)
        gfx_draw_rect(ctx, rect);
}

void gfx_draw_line(gfx_context_t *ctx, gfx_line_t line, gfx_color_t color)
{
    int32_t x1 = (int32_t) line.x1;
    int32_t y1 = (int32_t) line.y1;
    int32_t x2 = (int32_t) line.x2;
    int32_t y2 = (int32_t) line.y2;
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int t = (int) line.thickness;
    int start_offset = -(t - 1) / 2;
    int end_offset = start_offset + t - 1;
    uint32_t packed_color = pack_color(color);

    while (1) {
        for (int i = start_offset; i <= end_offset; i++) {
            for (int j = start_offset; j <= end_offset; j++)
                if (x1 + i >= 0 && y1 + j >= 0) {
                    uint32_t px = (uint32_t) (x1 + i);
                    uint32_t py = (uint32_t) (y1 + j);
                    if (px < ctx->width && py < ctx->height && point_in_clip(ctx, px, py)) {
                        size_t idx = (size_t) py * ctx->width + (size_t) px;
                        _blend_pixel(&ctx->backbuffer[idx], packed_color);
                    }
                }
        }
        if (x1 == x2 && y1 == y2)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static inline uint8_t _mul_u8(uint8_t a, uint8_t b)
{
    return (uint8_t) ((a * b + 127) / 255);
}

void gfx_draw_bitmap(gfx_context_t *ctx, gfx_bitmap_t *bitmap, gfx_pos_t pos, gfx_color_t color)
{
    if (!ctx || !bitmap || !bitmap->data || bitmap->width == 0 || bitmap->height == 0)
        return;

    if (color.a == 0)
        return;

    uint32_t x1 = pos.x;
    uint32_t y1 = pos.y;
    uint32_t x2 = pos.x + bitmap->width;
    uint32_t y2 = pos.y + bitmap->height;

    if (x1 < ctx->clip_rect.x)
        x1 = ctx->clip_rect.x;
    if (y1 < ctx->clip_rect.y)
        y1 = ctx->clip_rect.y;
    if (x2 > ctx->clip_rect.x + ctx->clip_rect.width)
        x2 = ctx->clip_rect.x + ctx->clip_rect.width;
    if (y2 > ctx->clip_rect.y + ctx->clip_rect.height)
        y2 = ctx->clip_rect.y + ctx->clip_rect.height;

    if (x2 > ctx->width)
        x2 = (uint32_t) ctx->width;
    if (y2 > ctx->height)
        y2 = (uint32_t) ctx->height;

    if (x1 >= x2 || y1 >= y2)
        return;

    uint32_t src_x_start = x1 - pos.x;
    uint32_t src_y_start = y1 - pos.y;
    uint32_t draw_width = x2 - x1;

    for (uint32_t y = 0; y < y2 - y1; y++) {
        const uint8_t *src_row = bitmap->data + (size_t) (src_y_start + y) * bitmap->width
                                 + src_x_start;
        uint32_t *dst_row = ctx->backbuffer + (size_t) (y1 + y) * ctx->width + x1;

        for (uint32_t x = 0; x < draw_width; x++) {
            uint8_t level = src_row[x];
            if (level == 0)
                continue;
            uint8_t a = _mul_u8(level, color.a);
            _blend_pixel(
                &dst_row[x],
                ((uint32_t) a << 24) | ((uint32_t) color.r << 16) | ((uint32_t) color.g << 8)
                    | (uint32_t) color.b);
        }
    }
}

void gfx_draw_colored_bitmap(gfx_context_t *ctx, gfx_colored_bitmap_t *bitmap, gfx_pos_t pos)
{
    if (!ctx || !bitmap || !bitmap->data || bitmap->width == 0 || bitmap->height == 0)
        return;

    uint32_t x1 = pos.x;
    uint32_t y1 = pos.y;
    uint32_t x2 = pos.x + bitmap->width;
    uint32_t y2 = pos.y + bitmap->height;

    if (x1 < ctx->clip_rect.x)
        x1 = ctx->clip_rect.x;
    if (y1 < ctx->clip_rect.y)
        y1 = ctx->clip_rect.y;
    if (x2 > ctx->clip_rect.x + ctx->clip_rect.width)
        x2 = ctx->clip_rect.x + ctx->clip_rect.width;
    if (y2 > ctx->clip_rect.y + ctx->clip_rect.height)
        y2 = ctx->clip_rect.y + ctx->clip_rect.height;

    if (x2 > ctx->width)
        x2 = (uint32_t) ctx->width;
    if (y2 > ctx->height)
        y2 = (uint32_t) ctx->height;

    if (x1 >= x2 || y1 >= y2)
        return;

    uint32_t src_x_start = x1 - pos.x;
    uint32_t src_y_start = y1 - pos.y;
    uint32_t draw_width = x2 - x1;

    for (uint32_t y = 0; y < y2 - y1; y++) {
        const uint32_t *src_row = bitmap->data + (size_t) (src_y_start + y) * bitmap->width
                                  + src_x_start;
        uint32_t *dst_row = ctx->backbuffer + (size_t) (y1 + y) * ctx->width + x1;

        /* Fast path: check if entire row is fully opaque and copy in bulk */
        uint32_t opaque_mask = 0xFF000000;
        for (uint32_t x = 0; x < draw_width; x++)
            opaque_mask &= src_row[x];

        if (opaque_mask >= 0xFF000000) {
            memcpy(dst_row, src_row, draw_width * sizeof(uint32_t));
        } else {
            for (uint32_t x = 0; x < draw_width; x++)
                _blend_pixel(&dst_row[x], src_row[x]);
        }
    }
}
