/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libgfx/types.h>

/* Use libgfx types */
typedef gfx_color_t color_t;

/* Framebuffer structure (compatibility wrapper) */
typedef struct
{
    uint32_t *framebuffer;
    size_t width;
    size_t height;
} framebuffer_t;

/* Rectangle structure - keeping local for now to avoid mass rename of w/h to width/height */
typedef struct
{
    int x, y, w, h;
} rect_t;

/* Point structure */
typedef struct
{
    int x, y;
} point_t;

/* Create a color from components */
/* gfx_color_t is {a, r, g, b} */
#define COLOR_MAKE(r, g, b, a) ((color_t){(a), (r), (g), (b)})

/* Convert color to uint32_t */
#define COLOR_TO_UINT32(c) (((uint32_t)(c).a << 24) | ((uint32_t)(c).r << 16) | ((uint32_t)(c).g << 8) | (uint32_t)(c).b)

/* Dark theme colors */
#define COLOR_WHITE COLOR_MAKE(255, 255, 255, 255)
#define COLOR_BLACK COLOR_MAKE(0, 0, 0, 255)
#define COLOR_GRAY COLOR_MAKE(90, 90, 90, 255)
#define COLOR_DARK_GRAY COLOR_MAKE(45, 45, 45, 255)
#define COLOR_LIGHT_GRAY COLOR_MAKE(140, 140, 140, 255)
#define COLOR_PLATINUM COLOR_MAKE(60, 60, 65, 255)    /* Main UI color - dark gray */
#define COLOR_HIGHLIGHT COLOR_MAKE(0, 122, 255, 255)  /* Selection blue */
#define COLOR_DESKTOP COLOR_MAKE(30, 30, 35, 255)     /* Dark desktop */
#define COLOR_TITLE_START COLOR_MAKE(80, 80, 85, 255) /* Title bar gradient */
#define COLOR_TITLE_END COLOR_MAKE(50, 50, 55, 255)
#define COLOR_SHADOW COLOR_MAKE(0, 0, 0, 255)
#define COLOR_BEVEL_LIGHT COLOR_MAKE(100, 100, 105, 255)
#define COLOR_BEVEL_DARK COLOR_MAKE(35, 35, 40, 255)
#define COLOR_TEXT COLOR_MAKE(230, 230, 230, 255)     /* Light text for dark bg */
#define COLOR_TEXT_DIM COLOR_MAKE(130, 130, 130, 255) /* Dimmed text */

/* Pattern for backgrounds */
#define PATTERN_NONE 0
#define PATTERN_DOTS 1
#define PATTERN_LINES 2
#define PATTERN_CHECKER 3
#define PATTERN_SOLID 4

/* Helper to convert rect_t to gfx_rect_t */
#define TO_GFX_RECT(r) ((gfx_rect_t){(r).x, (r).y, (r).w, (r).h, {0,0,0,0}, 0})

/* Global graphics context and font */
#include <libgfx/fonts.h>
extern gfx_context_t g_ctx;
extern gfx_font_t g_font;
