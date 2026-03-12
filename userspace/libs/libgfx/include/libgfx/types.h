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
    uint32_t x, y;
} gfx_pos_t;

typedef struct
{
    uint32_t x, y, width, height;
} gfx_area_t;

typedef struct
{
    uint32_t x, y;
    uint32_t width, height;
    gfx_color_t border_color;
    uint32_t border_thickness;
} gfx_rect_t;

typedef struct
{
    uint32_t width, height;
    uint8_t *data;
} gfx_bitmap_t;

typedef struct
{
    uint32_t width, height;
    uint32_t *data;
} gfx_colored_bitmap_t;

typedef struct
{
    uint32_t *framebuffer;
    uint32_t *backbuffer;
    size_t width;
    size_t height;
    size_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint32_t target_fps;
    uint64_t frame_start_ticks;
    uint64_t last_frame_ticks;
    uint64_t fps_last_update_ticks;
    uint32_t fps_frame_count;
    uint32_t fps_last_value;
    gfx_area_t clip_rect;
} gfx_context_t;

typedef struct
{
    uint32_t x1, y1;
    uint32_t x2, y2;
    uint32_t thickness;
} gfx_line_t;
