/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t a, r, g, b;
} gfx_color_t;

typedef struct
{
    int x, y;
} gfx_pos_t;

typedef struct
{
    int x, y;
    int width, height;
    gfx_color_t border_color;
    int border_thickness;
} gfx_rect_t;

typedef struct
{
    int width, height;
    uint8_t *data;
} gfx_bitmap_t;

typedef struct
{
    int width, height;
    uint32_t *data;
} gfx_colored_bitmap_t;

typedef struct
{
    uint32_t *framebuffer;
    uint32_t *backbuffer;
    size_t width;
    size_t height;
    gfx_rect_t clip_rect;
} gfx_context_t;

typedef struct
{
    int x1, y1;
    int x2, y2;
    gfx_color_t color;
    int thickness;
} gfx_line_t;
