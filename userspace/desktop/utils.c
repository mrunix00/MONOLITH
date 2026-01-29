/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./utils.h"
#include "./magic.h"

#include <libgfx.h>

void draw_box(gfx_context_t *context, gfx_rect_t rect)
{
    gfx_draw_filled_rect(
        context,
        (gfx_rect_t) {
            .x = rect.x,
            .y = rect.y,
            .width = rect.width,
            .height = rect.height,
            .border_thickness = BORDER_THICKNESS,
            .border_color = BORDER_COLOR,
        },
        SURFACE_COLOR);
    gfx_draw_rect(
        context,
        (gfx_rect_t) {
            .x = rect.x + BORDER_THICKNESS,
            .y = rect.y + BORDER_THICKNESS,
            .width = rect.width - BORDER_THICKNESS * 2,
            .height = rect.height - BORDER_THICKNESS * 2,
            .border_color = (gfx_color_t) {.a = 0x33, .r = 0xff, .g = 0xff, .b = 0xff},
            .border_thickness = BORDER_THICKNESS,
        });
}

void draw_transparent_box(gfx_context_t *context, gfx_rect_t rect)
{
    gfx_draw_filled_rect(
        context,
        (gfx_rect_t) {
            .x = rect.x,
            .y = rect.y,
            .width = rect.width,
            .height = rect.height,
            .border_color = BORDER_COLOR,
            .border_thickness = BORDER_THICKNESS,
        },
        (gfx_color_t) {.a = 0xcc, .r = 0x21, .g = 0x21, .b = 0x21});
    gfx_draw_rect(
        context,
        (gfx_rect_t) {
            .x = rect.x + BORDER_THICKNESS,
            .y = rect.y + BORDER_THICKNESS,
            .width = rect.width - BORDER_THICKNESS * 2,
            .height = rect.height - BORDER_THICKNESS * 2,
            .border_color = (gfx_color_t) {.a = 0x33, .r = 0xff, .g = 0xff, .b = 0xff},
            .border_thickness = BORDER_THICKNESS,
        });
}
