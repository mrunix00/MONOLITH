/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libgfx/fonts.h>

gfx_font_t gfx_load_vga_font(uint8_t *atlas, uint8_t width, uint8_t height)
{
    return (gfx_font_t) {
        .data.vga_atlas = atlas,
        .params.vga_params.height = height,
        .params.vga_params.width = width,
        .type = GFX_FONT_VGA,
    };
}

static uint8_t *_get_font_glyph(gfx_font_t *font, char c)
{
    return font->data.vga_atlas + (unsigned char) c * font->params.vga_params.height;
}

void _vga_draw_char(gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    const uint8_t *glyph = _get_font_glyph(font, c);
    for (int y = 0; y < font->params.vga_params.height; y++) {
        uint8_t bits = glyph[y];
        for (int x = 0; x < font->params.vga_params.width; x++) {
            if (bits & (0x80 >> x))
                gfx_draw_pixel(ctx, (gfx_pos_t) {.x = pos.x + x, .y = pos.y + y}, color);
        }
    }
}

int _vga_get_char_width(gfx_font_t *font, char c)
{
    return font->params.vga_params.width;
}
