/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stdint.h>

typedef struct
{
    uint32_t width, height;
} gfx_font_size_t;

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    int32_t x_offset;
    int32_t y_offset;
    uint32_t x_advance;
} gfx_glyph_t;

typedef struct
{
    gfx_font_size_t size;
    uint8_t first_char;
    uint8_t last_char;
    const uint8_t *atlas;
    gfx_font_size_t atlas_size;
    const gfx_glyph_t *glyphs;
} gfx_font_t;

gfx_font_t gfx_load_font(
    const uint8_t *atlas,
    gfx_font_size_t atlas_size,
    const gfx_glyph_t *glyphs,
    uint8_t first_char,
    uint8_t last_char,
    gfx_font_size_t size);

void gfx_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);

void gfx_draw_text(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, const char *);

void gfx_draw_text_centered(gfx_context_t *, gfx_font_t *, gfx_area_t, gfx_color_t, const char *);

uint32_t gfx_get_char_width(gfx_font_t *font, char c);

uint32_t gfx_get_char_height(gfx_font_t *font, char c);

uint32_t gfx_get_text_width(gfx_font_t *font, const char *text);

uint32_t gfx_get_text_height(gfx_font_t *font, const char *text);

gfx_area_t gfx_get_text_area(gfx_font_t *font, const char *text);
