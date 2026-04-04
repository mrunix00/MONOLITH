/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <stdbool.h>

static inline uint8_t _mul_u8(uint8_t a, uint8_t b)
{
    return (uint8_t) ((a * b + 127) / 255);
}

static const gfx_glyph_t *_get_glyph(gfx_font_t *font, char c)
{
    if (!font || !font->glyphs)
        return NULL;

    uint8_t uc = (uint8_t) c;
    uint8_t first = font->first_char;
    uint8_t last = font->last_char;
    if (uc < first || uc > last) {
        uint8_t space = (uint8_t) ' ';
        if (space >= first && space <= last)
            return &font->glyphs[space - first];
        return NULL;
    }

    return &font->glyphs[uc - first];
}

static uint32_t _get_advance(const gfx_glyph_t *glyph)
{
    if (glyph->x_advance > 0)
        return glyph->x_advance;
    return glyph->width;
}

gfx_font_t gfx_load_font(
    const uint8_t *atlas,
    gfx_font_size_t atlas_size,
    const gfx_glyph_t *glyphs,
    uint8_t first_char,
    uint8_t last_char,
    gfx_font_size_t size)
{
    return (gfx_font_t){
        .size = size,
        .first_char = first_char,
        .last_char = last_char,
        .atlas = atlas,
        .atlas_size = atlas_size,
        .glyphs = glyphs,
    };
}

void gfx_draw_char(gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    if (!ctx || !font || !font->atlas)
        return;

    const gfx_glyph_t *glyph = _get_glyph(font, c);
    if (!glyph || glyph->width == 0 || glyph->height == 0)
        return;

    uint32_t atlas_w = font->atlas_size.width;
    uint32_t atlas_h = font->atlas_size.height;
    if (atlas_w == 0 || atlas_h == 0)
        return;

    for (uint32_t y = 0; y < glyph->height; y++) {
        uint32_t src_y = glyph->y + y;
        if (src_y >= atlas_h)
            continue;
        const uint8_t *row = font->atlas + (size_t) src_y * atlas_w;
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
            gfx_draw_pixel(ctx, (gfx_pos_t){(uint32_t) dst_x, (uint32_t) dst_y}, out);
        }
    }
}

uint32_t gfx_get_char_width(gfx_font_t *font, char c)
{
    const gfx_glyph_t *glyph = _get_glyph(font, c);
    return _get_advance(glyph);
}

uint32_t gfx_get_char_height(gfx_font_t *font, char c)
{
    const gfx_glyph_t *glyph = _get_glyph(font, c);
    return glyph->height;
}

void gfx_draw_text(
    gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, const char *text)
{
    gfx_pos_t current_pos = pos;
    for (const char *p = text; *p; p++) {
        gfx_draw_char(ctx, font, current_pos, color, *p);
        current_pos.x += gfx_get_char_width(font, *p);
    }
}

uint32_t gfx_get_text_width(gfx_font_t *font, const char *text)
{
    uint32_t width = 0;
    for (const char *p = text; *p; p++)
        width += gfx_get_char_width(font, *p);
    return width;
}

uint32_t gfx_get_text_height(gfx_font_t *font, const char *text) {
    uint32_t height = 0;
    for (const char *p = text; *p; p++) {
        uint32_t char_height = gfx_get_char_height(font, *p);
        if (char_height > height)
            height = char_height;
    }
    return height;
}

gfx_area_t gfx_get_text_area(gfx_font_t *font, const char *text)
{
    if (!text || text[0] == '\0')
        return (gfx_area_t){0};

    int32_t min_x = 0;
    int32_t min_y = 0;
    int32_t max_x = 0;
    int32_t max_y = 0;
    bool has_bounds = false;

    int32_t pen_x = 0;
    for (const char *p = text; *p; p++) {
        const gfx_glyph_t *glyph = _get_glyph(font, *p);
        uint32_t advance = _get_advance(glyph);

        if (glyph && glyph->width > 0 && glyph->height > 0) {
            int32_t glyph_left = pen_x + glyph->x_offset;
            int32_t glyph_top = glyph->y_offset;
            int32_t glyph_right = glyph_left + (int32_t) glyph->width;
            int32_t glyph_bottom = glyph_top + (int32_t) glyph->height;

            if (!has_bounds) {
                min_x = glyph_left;
                min_y = glyph_top;
                max_x = glyph_right;
                max_y = glyph_bottom;
                has_bounds = true;
            } else {
                if (glyph_left < min_x)
                    min_x = glyph_left;
                if (glyph_top < min_y)
                    min_y = glyph_top;
                if (glyph_right > max_x)
                    max_x = glyph_right;
                if (glyph_bottom > max_y)
                    max_y = glyph_bottom;
            }
        }

        pen_x += (int32_t) advance;
    }

    uint32_t ascent = min_y < 0 ? (uint32_t) -min_y : 0;
    uint32_t descent = max_y > 0 ? (uint32_t) max_y : 0;
    uint32_t height = has_bounds ? ascent + descent : 0;

    return (gfx_area_t){
        .x = min_x < 0 ? (uint32_t) -min_x : 0,
        .y = ascent,
        .width = pen_x > 0 ? (uint32_t) pen_x : 0,
        .height = height,
    };
}

void gfx_draw_text_centered(
    gfx_context_t *ctx, gfx_font_t *font, gfx_area_t area, gfx_color_t color, const char *text)
{
    gfx_area_t text_area = gfx_get_text_area(font, text);
    uint32_t x = area.width > text_area.width ? area.x + (area.width - text_area.width) / 2
                                              : area.x;
    uint32_t y = area.height > text_area.height ? area.y + (area.height - text_area.height) / 2
                                                : area.y;
    gfx_draw_text(ctx, font, (gfx_pos_t){x + text_area.x, y + text_area.y}, color, text);
}
