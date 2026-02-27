/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/fonts.h>
#include <libgfx/types.h>

gfx_context_t gfx_init_screen();

void gfx_set_target_fps(gfx_context_t *ctx, uint32_t fps);

void gfx_begin_frame(gfx_context_t *ctx);

void gfx_end_frame(gfx_context_t *ctx);

void gfx_draw_fps_counter(gfx_context_t *ctx, gfx_font_t *font, gfx_color_t color, gfx_pos_t pos);

void gfx_deinit(gfx_context_t *ctx);

void gfx_clear(gfx_context_t *ctx, gfx_color_t color);

void gfx_draw_rect(gfx_context_t *ctx, gfx_rect_t rect);

void gfx_draw_filled_rect(gfx_context_t *ctx, gfx_rect_t rect, gfx_color_t fill);

void gfx_draw_line(gfx_context_t *ctx, gfx_line_t line, gfx_color_t color);

void gfx_draw_pixel(gfx_context_t *ctx, gfx_pos_t pos, gfx_color_t color);

void gfx_set_clip(gfx_context_t *ctx, gfx_rect_t rect);

void gfx_reset_clip(gfx_context_t *ctx);

void gfx_draw_bitmap(gfx_context_t *, gfx_bitmap_t *, gfx_pos_t, gfx_color_t);

void gfx_draw_colored_bitmap(gfx_context_t *, gfx_colored_bitmap_t *, gfx_pos_t);
