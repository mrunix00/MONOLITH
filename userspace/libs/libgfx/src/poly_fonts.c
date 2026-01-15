/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libgfx/fonts.h>
#include <stdint.h>

static inline uint8_t _mul_u8(uint8_t a, uint8_t b)
{
    return (uint8_t) ((a * b + 127) / 255);
}

gfx_font_t gfx_load_polyspace_font(
    const uint8_t *atlas,
    gfx_font_size_t atlas_size,
    const gfx_glyph_t *glyphs,
    uint8_t first_char,
    uint8_t last_char,
    gfx_font_size_t size)
{
    return (gfx_font_t) {
        .type = GFX_FONT_POLYSPACE,
        .params.polyspace =
            {
                .size = size,
                .first_char = first_char,
                .last_char = last_char,
            },
        .data.polyspace =
            {
                .atlas = atlas,
                .atlas_size = atlas_size,
                .glyphs = glyphs,
            },
    };
}

static const gfx_glyph_t *_polyspace_get_glyph(gfx_font_t *font, char c)
{
    if (!font || !font->data.polyspace.glyphs)
        return NULL;

    uint8_t uc = (uint8_t) c;
    uint8_t first = font->params.polyspace.first_char;
    uint8_t last = font->params.polyspace.last_char;
    if (uc < first || uc > last) {
        uint8_t space = (uint8_t) ' ';
        if (space >= first && space <= last)
            return &font->data.polyspace.glyphs[space - first];
        return NULL;
    }

    return &font->data.polyspace.glyphs[uc - first];
}

void _polyspace_draw_char(
    gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    if (!ctx || !font || !font->data.polyspace.atlas)
        return;

    const gfx_glyph_t *glyph = _polyspace_get_glyph(font, c);
    if (!glyph || glyph->width == 0 || glyph->height == 0)
        return;

    uint32_t atlas_w = font->data.polyspace.atlas_size.width;
    uint32_t atlas_h = font->data.polyspace.atlas_size.height;
    if (atlas_w == 0 || atlas_h == 0)
        return;

    for (uint32_t y = 0; y < glyph->height; y++) {
        uint32_t src_y = glyph->y + y;
        if (src_y >= atlas_h)
            continue;
        const uint8_t *row = font->data.polyspace.atlas + (size_t) src_y * atlas_w;
        for (uint32_t x = 0; x < glyph->width; x++) {
            uint32_t src_x = glyph->x + x;
            if (src_x >= atlas_w)
                continue;
            uint8_t alpha = row[src_x];
            if (alpha == 0)
                continue;
            gfx_color_t out = color;
            out.a = _mul_u8(alpha, color.a);
            if (out.a == 0)
                continue;
            int32_t dst_x = (int32_t) pos.x + glyph->x_offset + (int32_t) x;
            int32_t dst_y = (int32_t) pos.y + glyph->y_offset + (int32_t) y;
            if (dst_x < 0 || dst_y < 0)
                continue;
            gfx_draw_pixel(ctx, (gfx_pos_t) {(uint32_t) dst_x, (uint32_t) dst_y}, out);
        }
    }
}

uint32_t _polyspace_get_char_width(gfx_font_t *font, char c)
{
    const gfx_glyph_t *glyph = _polyspace_get_glyph(font, c);
    if (!glyph)
        return 0;
    if (glyph->x_advance > 0)
        return glyph->x_advance;
    return glyph->width;
}
