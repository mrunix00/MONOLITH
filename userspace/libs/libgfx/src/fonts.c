/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <libgfx/fonts.h>

extern void _ttf_unload_font(gfx_font_t *font);
void gfx_unload_font(gfx_font_t *font)
{
    switch (font->type) {
    case GFX_FONT_MONOSPACE:
    case GFX_FONT_POLYSPACE:
    case GFX_FONT_INVALID:
        return;
    }
}

extern void _ttf_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);
extern void _monospace_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);
extern void _polyspace_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);
void gfx_draw_char(gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, char c)
{
    switch (font->type) {
    case GFX_FONT_INVALID:
        return;
    case GFX_FONT_MONOSPACE:
        return _monospace_draw_char(ctx, font, pos, color, c);
    case GFX_FONT_POLYSPACE:
        return _polyspace_draw_char(ctx, font, pos, color, c);
    }
}

extern int _monospace_get_char_width(gfx_font_t *);
extern int _polyspace_get_char_width(gfx_font_t *, char);
int gfx_get_char_width(gfx_font_t *font, char c)
{
    switch (font->type) {
    case GFX_FONT_INVALID:
        return 0;
    case GFX_FONT_MONOSPACE:
        return _monospace_get_char_width(font);
    case GFX_FONT_POLYSPACE:
        return _polyspace_get_char_width(font, c);
    }
    return 0;
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

int gfx_get_text_width(gfx_font_t *font, const char *text)
{
    int width = 0;
    for (const char *p = text; *p; p++)
        width += gfx_get_char_width(font, *p);
    return width;
}

void gfx_draw_text_centered(
    gfx_context_t *ctx, gfx_font_t *font, gfx_rect_t rect, gfx_color_t color, const char *text)
{
    int text_w = gfx_get_text_width(font, text);
    int text_h = 0;
    switch (font->type) {
    case GFX_FONT_INVALID:
        break;
    case GFX_FONT_MONOSPACE:
        text_h = font->params.monospace.size.height;
        break;
    case GFX_FONT_POLYSPACE:
        text_h = font->params.polyspace.size.height;
        break;
    }

    int x = rect.x + (rect.width - text_w) / 2;
    int y = rect.y + (rect.height - text_h) / 2;

    gfx_draw_text(ctx, font, (gfx_pos_t) {x, y}, color, text);
}
