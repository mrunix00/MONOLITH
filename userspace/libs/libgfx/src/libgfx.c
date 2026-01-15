/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define abs(x) ((x) < 0 ? -(x) : (x))
#define pack_color(c) ((c.a << 24) | (c.r << 16) | (c.g << 8) | c.b)

typedef struct
{
    uint32_t *framebuffer;
    size_t width;
    size_t height;
} _framebuffer_t;

gfx_context_t gfx_init_screen()
{
    _framebuffer_t fb;
    syscall1(SYSCALL_REQUEST_FB, (long) &fb);
    gfx_context_t ctx
        = {.framebuffer = fb.framebuffer,
           .width = fb.width,
           .height = fb.height,
           .backbuffer = malloc(fb.width * fb.height * sizeof(uint32_t)),
           .clip_rect = {0, 0, (uint32_t) fb.width, (uint32_t) fb.height, {0, 0, 0, 0}, 0}};
    return ctx;
}

void gfx_deinit(gfx_context_t *context)
{
    free(context->backbuffer);
}

void gfx_flush(gfx_context_t *context)
{
    memcpy(
        context->framebuffer,
        context->backbuffer,
        context->width * context->height * sizeof(uint32_t));
}

void gfx_clear(gfx_context_t *ctx, gfx_color_t color)
{
    uint32_t packed = pack_color(color);
    for (size_t i = 0; i < ctx->width * ctx->height; i++) {
        ctx->backbuffer[i] = packed;
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
        if (color.a == 0)
            return;
        else if (color.a == 255) {
            ctx->backbuffer[idx] = pack_color(color);
            return;
        }

        uint32_t dst = ctx->backbuffer[idx];
        uint8_t dst_a = (uint8_t) (dst >> 24);
        uint8_t dst_r = (uint8_t) (dst >> 16);
        uint8_t dst_g = (uint8_t) (dst >> 8);
        uint8_t dst_b = (uint8_t) dst;
        uint32_t inv_a = 255 - color.a;

        uint8_t out_a = (uint8_t) ((color.a + (dst_a * inv_a + 127) / 255) & 0xFF);
        uint8_t out_r = (uint8_t) ((color.r * color.a + dst_r * inv_a + 127) / 255);
        uint8_t out_g = (uint8_t) ((color.g * color.a + dst_g * inv_a + 127) / 255);
        uint8_t out_b = (uint8_t) ((color.b * color.a + dst_b * inv_a + 127) / 255);
        gfx_color_t out_color = {out_a, out_r, out_g, out_b};
        ctx->backbuffer[idx] = pack_color(out_color);
    }
}

void gfx_set_clip(gfx_context_t *ctx, gfx_rect_t rect)
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
        (gfx_line_t) {
            rect.x,
            rect.y,
            rect.x + rect.width - 1,
            rect.y,
            rect.border_thickness,
        },
        rect.border_color);
    gfx_draw_line(
        ctx,
        (gfx_line_t) {
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
        (gfx_line_t) {
            rect.x,
            rect.y,
            rect.x,
            rect.y + rect.height - 1,
            rect.border_thickness,
        },
        rect.border_color);
    gfx_draw_line(
        ctx,
        (gfx_line_t) {
            rect.x + rect.width - 1,
            rect.y,
            rect.x + rect.width - 1,
            rect.y + rect.height - 1,
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

    uint32_t c = pack_color(fill);

    for (uint32_t y = y1; y < y2; y++) {
        uint32_t *row = ctx->backbuffer + (size_t) y * ctx->width;
        for (uint32_t x = x1; x < x2; x++) {
            row[x] = c;
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

    /* Using gfx_draw_pixel which handles clipping */
    while (1) {
        for (int i = start_offset; i <= end_offset; i++) {
            for (int j = start_offset; j <= end_offset; j++)
                if (x1 + i >= 0 && y1 + j >= 0)
                    gfx_draw_pixel(
                        ctx,
                        (gfx_pos_t) {(uint32_t) (x1 + i), (uint32_t) (y1 + j)},
                        color);
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

    uint32_t row_bytes = (bitmap->width + 7) / 8;

    for (uint32_t y = y1; y < y2; y++) {
        uint32_t src_y = y - pos.y;
        const uint8_t *row = bitmap->data + (size_t) src_y * row_bytes;
        for (uint32_t x = x1; x < x2; x++) {
            uint32_t src_x = x - pos.x;
            uint32_t byte_idx = src_x / 8;
            uint32_t bit_idx = 7 - (src_x % 8);
            if (row[byte_idx] & (uint8_t) (1u << bit_idx))
                gfx_draw_pixel(ctx, (gfx_pos_t) {x, y}, color);
        }
    }
}

void gfx_draw_colored_bitmap(
    gfx_context_t *ctx, gfx_colored_bitmap_t *bitmap, gfx_pos_t pos, gfx_color_t color)
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

    for (uint32_t y = y1; y < y2; y++) {
        uint32_t src_y = y - pos.y;
        const uint32_t *row = bitmap->data + (size_t) src_y * bitmap->width;
        for (uint32_t x = x1; x < x2; x++) {
            uint32_t src_x = x - pos.x;
            uint32_t src = row[src_x];
            uint8_t src_a = (uint8_t) (src >> 24);
            if (src_a == 0)
                continue;

            gfx_color_t out = {
                .a = _mul_u8(src_a, color.a),
                .r = _mul_u8((uint8_t) (src >> 16), color.r),
                .g = _mul_u8((uint8_t) (src >> 8), color.g),
                .b = _mul_u8((uint8_t) src, color.b),
            };
            if (out.a == 0)
                continue;
            gfx_draw_pixel(ctx, (gfx_pos_t) {x, y}, out);
        }
    }
}
