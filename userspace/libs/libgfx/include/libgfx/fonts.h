/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stdint.h>

typedef enum {
    GFX_FONT_INVALID = 0,
    GFX_FONT_VGA,
    GFX_FONT_TTF,
} gfx_font_type_t;

typedef struct
{
    int width;
    int height;
    int xoff;
    int yoff;
    int advance;
    uint8_t *bitmap;
} gfx_ttf_glyph_t;

typedef struct
{
    gfx_font_type_t type;
    union {
        struct
        {
            int ascent, descent, lineGap;
        } ttf_params;
        struct
        {
            int width, height;
        } vga_params;
    } params;
    union {
        uint8_t *vga_atlas;
        gfx_ttf_glyph_t *ttf_glyphs;
    } data;
} gfx_font_t;

gfx_font_t gfx_load_ttf(void *data, float size);

gfx_font_t gfx_load_vga_font(uint8_t *atlas, uint8_t width, uint8_t height);

void gfx_unload_font(gfx_font_t *font);

void gfx_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);

void gfx_draw_text(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, const char *);

void gfx_draw_text_centered(gfx_context_t *, gfx_font_t *, gfx_rect_t, gfx_color_t, const char *);

int gfx_get_char_width(gfx_font_t *font, char c);

int gfx_get_text_width(gfx_font_t *font, const char *text);
