/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stdint.h>

typedef enum {
    GFX_FONT_INVALID = 0,
    GFX_FONT_MONOSPACE,
    GFX_FONT_POLYSPACE,
} gfx_font_type_t;

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
    gfx_font_type_t type;
    union {
        struct
        {
            gfx_font_size_t size;
        } monospace;
        struct
        {
            gfx_font_size_t size;
            uint8_t first_char;
            uint8_t last_char;
        } polyspace;
    } params;
    union {
        const uint8_t *monospace_atlas;
        struct
        {
            const uint8_t *atlas;
            gfx_font_size_t atlas_size;
            const gfx_glyph_t *glyphs;
        } polyspace;
    } data;
} gfx_font_t;

gfx_font_t gfx_load_ttf(void *data, float size);

gfx_font_t gfx_load_monospace_font(const uint8_t *atlas, uint8_t width, uint8_t height);

gfx_font_t gfx_load_polyspace_font(
    const uint8_t *atlas,
    gfx_font_size_t atlas_size,
    const gfx_glyph_t *glyphs,
    uint8_t first_char,
    uint8_t last_char,
    gfx_font_size_t size);

void gfx_unload_font(gfx_font_t *font);

void gfx_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, char);

void gfx_draw_text(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, const char *);

void gfx_draw_text_centered(gfx_context_t *, gfx_font_t *, gfx_rect_t, gfx_color_t, const char *);

uint32_t gfx_get_char_width(gfx_font_t *font, char c);

uint32_t gfx_get_text_width(gfx_font_t *font, const char *text);
