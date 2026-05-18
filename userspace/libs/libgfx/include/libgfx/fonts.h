/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stb_truetype.h>

typedef struct
{
    uint32_t codepoint;
    int width;
    int height;
    int x_offset;
    int y_offset;
    int x_advance;
    unsigned char *bitmap;
    size_t next;
} gfx_glyph_t;

typedef struct
{
    stbtt_fontinfo info;
    unsigned char *arena;
    size_t arena_size;
    size_t arena_capacity;
    unsigned char *data;
    int font_offset;
    float scale;
    uint32_t pixel_size;
    uint32_t line_height;
    size_t first_glyph;
    size_t last_glyph;
} gfx_font_t;

gfx_font_t gfx_load_font(const void *data, uint32_t font_size);

gfx_font_t gfx_load_font_from_file(const char *path, uint32_t font_size);

void gfx_unload_font(gfx_font_t *font);

void gfx_draw_char(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, uint32_t codepoint);

void gfx_draw_text(gfx_context_t *, gfx_font_t *, gfx_pos_t, gfx_color_t, const char *);

void gfx_draw_text_centered(gfx_context_t *, gfx_font_t *, gfx_area_t, gfx_color_t, const char *);

uint32_t gfx_get_char_width(gfx_font_t *font, uint32_t codepoint);

uint32_t gfx_get_char_height(gfx_font_t *font, uint32_t codepoint);

uint32_t gfx_get_text_width(gfx_font_t *font, const char *text);

uint32_t gfx_get_text_height(gfx_font_t *font, const char *text);

gfx_area_t gfx_get_text_area(gfx_font_t *font, const char *text);
