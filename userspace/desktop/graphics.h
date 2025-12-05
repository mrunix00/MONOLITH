/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include "types.h"

/* Initialize graphics system with framebuffer */
int graphics_init(void);

/* Get framebuffer info */
framebuffer_t *graphics_get_fb(void);

/* Clear screen with color */
void graphics_clear(color_t color);

/* Draw a filled rectangle */
void graphics_fill_rect(rect_t rect, color_t color);

/* Draw a rectangle outline */
void graphics_draw_rect(rect_t rect, color_t color);

/* Draw a horizontal line */
void graphics_hline(int x1, int x2, int y, color_t color);

/* Draw a vertical line */
void graphics_vline(int x, int y1, int y2, color_t color);

/* Draw a line between two points */
void graphics_line(int x1, int y1, int x2, int y2, color_t color);

/* Draw a single pixel */
void graphics_pixel(int x, int y, color_t color);

/* Draw text at position */
void graphics_draw_text(const char *text, int x, int y, color_t color);

/* Draw text centered in rect */
void graphics_draw_text_centered(const char *text, rect_t rect, color_t color);

/* Set clipping rectangle */
void graphics_set_clip(rect_t rect);

/* Reset clipping to full screen */
void graphics_reset_clip(void);

/* Draw patterned rectangle (for desktop backgrounds) */
void graphics_fill_pattern(rect_t rect, int pattern);

/* Draw a 3D-style raised button */
void graphics_draw_button(rect_t rect, bool pressed);

/* Copy backbuffer to framebuffer */
void graphics_present(void);

/* Get backbuffer for direct access */
uint32_t *graphics_get_backbuffer(void);
