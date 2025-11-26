/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <microui.h>
#include <stdint.h>
#include <string.h>

#include "./atlas.inl"
#include "./framebuffer.h"
#include "./renderer.h"

extern framebuffer_t fb;
extern uint32_t framebuffer[2073600];

static mu_Rect _clip_rect;

static inline uint32_t muColor2ARGB(mu_Color color)
{
    return (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
}

static void blit_quad(mu_Rect dst, mu_Rect src, mu_Color color)
{
    /* Check if destination is entirely outside the clip rectangle */
    if (dst.x + dst.w <= _clip_rect.x || dst.x >= _clip_rect.x + _clip_rect.w
        || dst.y + dst.h <= _clip_rect.y || dst.y >= _clip_rect.y + _clip_rect.h)
        return;

    /* Calculate visible region within destination */
    int start_dx = mu_max(0, _clip_rect.x - dst.x);
    int end_dx = mu_min(dst.w, _clip_rect.x + _clip_rect.w - dst.x);
    int start_dy = mu_max(0, _clip_rect.y - dst.y);
    int end_dy = mu_min(dst.h, _clip_rect.y + _clip_rect.h - dst.y);

    /* Skip if no visible region */
    if (start_dx >= end_dx || start_dy >= end_dy)
        return;

    /* Process visible pixels */
    for (int dy = start_dy; dy < end_dy; dy++) {
        size_t sy = src.y + (dy * src.h) / dst.h;
        size_t fy = dst.y + dy;
        for (int dx = start_dx; dx < end_dx; dx++) {
            size_t sx = src.x + (dx * src.w) / dst.w;
            size_t fx = dst.x + dx;

            uint8_t alpha = atlas_texture[sy * ATLAS_WIDTH + sx];
            if (alpha == 0)
                continue;

            /* Blend pixel with framebuffer */
            uint32_t *dst_px = &framebuffer[fy * fb.width + fx];
            uint32_t dst_col = *dst_px;
            uint8_t dr = (dst_col >> 16) & 0xFF;
            uint8_t dg = (dst_col >> 8) & 0xFF;
            uint8_t db = dst_col & 0xFF;
            uint8_t da = (dst_col >> 24) & 0xFF;

            uint8_t sa = (color.a * alpha) / 255;
            uint8_t out_a = sa + (da * (255 - sa)) / 255;
            uint8_t out_r = (color.r * sa + dr * (255 - sa)) / 255;
            uint8_t out_g = (color.g * sa + dg * (255 - sa)) / 255;
            uint8_t out_b = (color.b * sa + db * (255 - sa)) / 255;

            *dst_px = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void r_init()
{
    _clip_rect = mu_rect(0, 0, fb.width, fb.height);
}

void r_draw_rect(mu_Rect rect, mu_Color color)
{
    blit_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color)
{
    mu_Rect dst = {pos.x, pos.y, 0, 0};
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) {
            continue;
        }
        int chr = mu_min((unsigned char) *p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        blit_quad(dst, src, color);
        dst.x += dst.w;
    }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color)
{
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    blit_quad(mu_rect(x, y, src.w, src.h), src, color);
}

int r_get_text_width(const char *text, int len)
{
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80)
            continue;
        int chr = mu_min((unsigned char) *p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

int r_get_text_height(void)
{
    return 18;
}

void r_set_clip_rect(mu_Rect rect)
{
    if (rect.x < 0) {
        rect.w += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.h += rect.y;
        rect.y = 0;
    }

    if ((size_t) rect.x + rect.w > fb.width)
        rect.w = fb.width - rect.x;
    if ((size_t) rect.y + rect.h > fb.height)
        rect.h = fb.height - rect.y;

    if (rect.w < 0)
        rect.w = 0;
    if (rect.h < 0)
        rect.h = 0;

    _clip_rect = rect;
}

void r_clear(mu_Color clr)
{
    uint32_t color = muColor2ARGB(clr);
    for (size_t y = 0; y < fb.height; y++) {
        for (size_t x = 0; x < fb.width; x++) {
            framebuffer[y * fb.width + x] = color;
        }
    }
}

void r_present(void)
{
    memcpy(fb.framebuffer, framebuffer, sizeof(uint32_t) * fb.width * fb.height);
}
