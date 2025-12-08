/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "graphics.h"
#include "font.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Framebuffer info */
static framebuffer_t fb;

/* Double buffer */
static uint32_t *backbuffer = NULL;

/* Clipping rectangle */
static rect_t clip_rect;

int graphics_init(void)
{
    int result = syscall1(SYSCALL_REQUEST_FB, (long) &fb);
    if (result < 0) {
        return -1;
    }

    /* Calculate the number of pages needed for the backbuffer */
    size_t buffer_size = fb.width * fb.height * sizeof(uint32_t);
    size_t num_pages = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Allocate backbuffer using the alloc_pages syscall */
    backbuffer = (uint32_t *) alloc_pages(num_pages, ALLOC_PAGES_FLAG_RW);
    if (backbuffer == NULL) {
        return -1;
    }

    /* Initialize clip rect to full screen */
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.w = (int) fb.width;
    clip_rect.h = (int) fb.height;

    /* Clear backbuffer */
    memset(backbuffer, 0xFF, fb.width * fb.height * sizeof(uint32_t));

    return 0;
}

framebuffer_t *graphics_get_fb(void)
{
    return &fb;
}

static inline bool point_in_clip(int x, int y)
{
    return x >= clip_rect.x && x < clip_rect.x + clip_rect.w && y >= clip_rect.y
           && y < clip_rect.y + clip_rect.h;
}

static inline void set_pixel(int x, int y, uint32_t color)
{
    if (x >= 0 && x < (int) fb.width && y >= 0 && y < (int) fb.height) {
        if (point_in_clip(x, y)) {
            backbuffer[y * fb.width + x] = color;
        }
    }
}

void graphics_clear(color_t color)
{
    uint32_t c = COLOR_TO_UINT32(color);
    for (size_t i = 0; i < fb.width * fb.height; i++) {
        backbuffer[i] = c;
    }
}

void graphics_fill_rect(rect_t rect, color_t color)
{
    uint32_t c = COLOR_TO_UINT32(color);

    int x1 = rect.x;
    int y1 = rect.y;
    int x2 = rect.x + rect.w;
    int y2 = rect.y + rect.h;

    /* Clamp to screen */
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 > (int) fb.width)
        x2 = (int) fb.width;
    if (y2 > (int) fb.height)
        y2 = (int) fb.height;

    /* Clamp to clip rect */
    if (x1 < clip_rect.x)
        x1 = clip_rect.x;
    if (y1 < clip_rect.y)
        y1 = clip_rect.y;
    if (x2 > clip_rect.x + clip_rect.w)
        x2 = clip_rect.x + clip_rect.w;
    if (y2 > clip_rect.y + clip_rect.h)
        y2 = clip_rect.y + clip_rect.h;

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            backbuffer[y * fb.width + x] = c;
        }
    }
}

void graphics_draw_rect(rect_t rect, color_t color)
{
    /* Top and bottom lines */
    graphics_hline(rect.x, rect.x + rect.w - 1, rect.y, color);
    graphics_hline(rect.x, rect.x + rect.w - 1, rect.y + rect.h - 1, color);

    /* Left and right lines */
    graphics_vline(rect.x, rect.y, rect.y + rect.h - 1, color);
    graphics_vline(rect.x + rect.w - 1, rect.y, rect.y + rect.h - 1, color);
}

void graphics_hline(int x1, int x2, int y, color_t color)
{
    if (x1 > x2) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
    }

    uint32_t c = COLOR_TO_UINT32(color);
    for (int x = x1; x <= x2; x++) {
        set_pixel(x, y, c);
    }
}

void graphics_vline(int x, int y1, int y2, color_t color)
{
    if (y1 > y2) {
        int tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    uint32_t c = COLOR_TO_UINT32(color);
    for (int y = y1; y <= y2; y++) {
        set_pixel(x, y, c);
    }
}

void graphics_line(int x1, int y1, int x2, int y2, color_t color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;

    uint32_t c = COLOR_TO_UINT32(color);

    if (dx > dy) {
        int err = dx / 2;
        int y = y1;
        for (int x = x1; x != x2; x += sx) {
            set_pixel(x, y, c);
            err -= dy;
            if (err < 0) {
                y += sy;
                err += dx;
            }
        }
    } else {
        int err = dy / 2;
        int x = x1;
        for (int y = y1; y != y2; y += sy) {
            set_pixel(x, y, c);
            err -= dx;
            if (err < 0) {
                x += sx;
                err += dy;
            }
        }
    }
    set_pixel(x2, y2, c);
}

void graphics_pixel(int x, int y, color_t color)
{
    set_pixel(x, y, COLOR_TO_UINT32(color));
}

void graphics_draw_text(const char *text, int x, int y, color_t color)
{
    uint32_t c = COLOR_TO_UINT32(color);

    while (*text) {
        const uint8_t *glyph = font_get_glyph(*text);

        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (bits & (0x80 >> col)) {
                    set_pixel(x + col, y + row, c);
                }
            }
        }

        x += FONT_WIDTH;
        text++;
    }
}

void graphics_draw_text_centered(const char *text, rect_t rect, color_t color)
{
    int text_w = font_text_width(text);
    int text_h = font_text_height();

    int x = rect.x + (rect.w - text_w) / 2;
    int y = rect.y + (rect.h - text_h) / 2;

    graphics_draw_text(text, x, y, color);
}

void graphics_set_clip(rect_t rect)
{
    clip_rect = rect;

    /* Clamp to screen */
    if (clip_rect.x < 0) {
        clip_rect.w += clip_rect.x;
        clip_rect.x = 0;
    }
    if (clip_rect.y < 0) {
        clip_rect.h += clip_rect.y;
        clip_rect.y = 0;
    }
    if (clip_rect.x + clip_rect.w > (int) fb.width) {
        clip_rect.w = (int) fb.width - clip_rect.x;
    }
    if (clip_rect.y + clip_rect.h > (int) fb.height) {
        clip_rect.h = (int) fb.height - clip_rect.y;
    }
}

void graphics_reset_clip(void)
{
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.w = (int) fb.width;
    clip_rect.h = (int) fb.height;
}

void graphics_fill_pattern(rect_t rect, int pattern)
{
    uint32_t desktop = COLOR_TO_UINT32(COLOR_DESKTOP);
    uint32_t light = COLOR_TO_UINT32(COLOR_MAKE(38, 38, 43, 255)); /* Slightly lighter */
    uint32_t dark = COLOR_TO_UINT32(COLOR_MAKE(25, 25, 30, 255));  /* Slightly darker */

    int x1 = rect.x;
    int y1 = rect.y;
    int x2 = rect.x + rect.w;
    int y2 = rect.y + rect.h;

    /* Clamp to screen and clip */
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 > (int) fb.width)
        x2 = (int) fb.width;
    if (y2 > (int) fb.height)
        y2 = (int) fb.height;
    if (x1 < clip_rect.x)
        x1 = clip_rect.x;
    if (y1 < clip_rect.y)
        y1 = clip_rect.y;
    if (x2 > clip_rect.x + clip_rect.w)
        x2 = clip_rect.x + clip_rect.w;
    if (y2 > clip_rect.y + clip_rect.h)
        y2 = clip_rect.y + clip_rect.h;

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            uint32_t c;
            switch (pattern) {
            case PATTERN_DOTS:
                /* Dithered pattern */
                c = ((x + y) % 2 == 0) ? light : dark;
                break;
            case PATTERN_LINES:
                c = (y % 2 == 0) ? light : desktop;
                break;
            case PATTERN_CHECKER:
                c = (((x / 4) + (y / 4)) % 2 == 0) ? light : dark;
                break;
            case PATTERN_SOLID:
                c = desktop;
                break;
            default:
                c = desktop;
                break;
            }
            backbuffer[y * fb.width + x] = c;
        }
    }
}

void graphics_draw_button(rect_t rect, bool pressed)
{
    if (pressed) {
        /* Pressed button - darker with inverted bevel */
        graphics_fill_rect(rect, COLOR_DARK_GRAY);
        /* Inner shadow (top-left dark) */
        graphics_hline(rect.x + 1, rect.x + rect.w - 2, rect.y + 1, COLOR_BLACK);
        graphics_vline(rect.x + 1, rect.y + 1, rect.y + rect.h - 2, COLOR_BLACK);
        /* Outer frame */
        graphics_draw_rect(rect, COLOR_BLACK);
    } else {
        /* Normal button - dark theme raised 3D look */
        graphics_fill_rect(rect, COLOR_PLATINUM);

        /* Outer bevel - lighter on top-left */
        graphics_hline(rect.x + 1, rect.x + rect.w - 2, rect.y + 1, COLOR_BEVEL_LIGHT);
        graphics_vline(rect.x + 1, rect.y + 1, rect.y + rect.h - 2, COLOR_BEVEL_LIGHT);

        /* Inner shadow - dark on bottom-right */
        graphics_hline(rect.x + 2, rect.x + rect.w - 2, rect.y + rect.h - 2, COLOR_BEVEL_DARK);
        graphics_vline(rect.x + rect.w - 2, rect.y + 2, rect.y + rect.h - 2, COLOR_BEVEL_DARK);

        /* Outer frame */
        graphics_draw_rect(rect, COLOR_BLACK);
    }
}

void graphics_present(void)
{
    memcpy(fb.framebuffer, backbuffer, fb.width * fb.height * sizeof(uint32_t));
}

uint32_t *graphics_get_backbuffer(void)
{
    return backbuffer;
}
