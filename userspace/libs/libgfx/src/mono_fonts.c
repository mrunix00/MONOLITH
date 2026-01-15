/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libgfx/fonts.h>
#include <stdint.h>

gfx_font_t gfx_load_monospace_font(const uint8_t *atlas, uint8_t width, uint8_t height)
{
    return (gfx_font_t) {
        .data.monospace_atlas = atlas,
        .params.monospace.size.height = height,
        .params.monospace.size.width = width,
        .type = GFX_FONT_MONOSPACE,
    };
}

static uint8_t *_get_font_glyph(gfx_font_t *font, char c)
{
    return (uint8_t *) font->data.monospace_atlas
           + (size_t) (unsigned char) c * font->params.monospace.size.height;
}

void _monospace_draw_char(
    gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    const uint8_t *glyph = _get_font_glyph(font, c);
    for (uint32_t y = 0; y < font->params.monospace.size.height; y++) {
        uint8_t bits = glyph[y];
        for (uint32_t x = 0; x < font->params.monospace.size.width; x++) {
            if (bits & (0x80 >> x))
                gfx_draw_pixel(ctx, (gfx_pos_t) {.x = pos.x + x, .y = pos.y + y}, color);
        }
    }
}

uint32_t _monospace_get_char_width(gfx_font_t *font)
{
    return font->params.monospace.size.width;
}
