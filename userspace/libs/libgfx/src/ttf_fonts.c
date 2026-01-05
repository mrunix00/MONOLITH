/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libgfx/fonts.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

gfx_font_t gfx_load_ttf(void *ttf_data, float font_size)
{
    gfx_font_t font = {0};
    stbtt_fontinfo info;

    if (ttf_data == NULL || font_size <= 0.0f)
        return font;

    int offset = stbtt_GetFontOffsetForIndex(ttf_data, 0);
    if (offset < 0)
        return font;
    if (!stbtt_InitFont(&info, ttf_data, offset))
        return font;

    float scale = stbtt_ScaleForPixelHeight(&info, font_size);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

    font.type = GFX_FONT_TTF;
    font.params.ttf_params.ascent = (int) (ascent * scale + 0.5f);
    font.params.ttf_params.descent = (int) (-descent * scale + 0.5f);
    font.params.ttf_params.lineGap = (int) (line_gap * scale + 0.5f);

    font.data.ttf_glyphs = malloc(sizeof(gfx_ttf_glyph_t) * 256);
    if (font.data.ttf_glyphs == NULL)
        return (gfx_font_t) {0};

    memset(font.data.ttf_glyphs, 0, sizeof(gfx_ttf_glyph_t) * 256);

    for (int c = 0; c < 256; c++) {
        gfx_ttf_glyph_t *glyph = &font.data.ttf_glyphs[c];
        int width, height, xoff, yoff;
        unsigned char *bitmap
            = stbtt_GetCodepointBitmap(&info, 0, scale, c, &width, &height, &xoff, &yoff);
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&info, c, &advance, &lsb);

        glyph->width = width;
        glyph->height = height;
        glyph->xoff = xoff;
        glyph->yoff = yoff;
        glyph->advance = (int) (advance * scale + 0.5f);
        glyph->bitmap = bitmap;
    }

    return font;
}

void _ttf_draw_char(gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    gfx_ttf_glyph_t *glyph = &font->data.ttf_glyphs[(unsigned char) c];
    if (glyph->bitmap == NULL || glyph->width <= 0 || glyph->height <= 0)
        return;

    int baseline = pos.y + font->params.ttf_params.ascent;
    int start_x = pos.x + glyph->xoff;
    int start_y = baseline + glyph->yoff;

    for (int y = 0; y < glyph->height; y++) {
        int py = start_y + y;
        if (py < 0 || py >= (int) ctx->height)
            continue;
        if (py < ctx->clip_rect.y || py >= ctx->clip_rect.y + ctx->clip_rect.height)
            continue;

        for (int x = 0; x < glyph->width; x++) {
            int px = start_x + x;
            if (px < 0 || px >= (int) ctx->width)
                continue;
            if (px < ctx->clip_rect.x || px >= ctx->clip_rect.x + ctx->clip_rect.width)
                continue;

            uint8_t coverage = glyph->bitmap[y * glyph->width + x];
            if (coverage == 0)
                continue;

            gfx_draw_pixel(ctx, (gfx_pos_t) {.x = px, .y = py}, color);
        }
    }
}

int _ttf_get_char_width(gfx_font_t *font, char c)
{
    return font->data.ttf_glyphs[(unsigned char) c].advance;
}

void _ttf_unload_font(gfx_font_t *font)
{
    for (int i = 0; i < 256; i++) {
        if (font->data.ttf_glyphs[i].bitmap != NULL)
            stbtt_FreeBitmap(font->data.ttf_glyphs[i].bitmap, NULL);
    }
    free(font->data.ttf_glyphs);
}
