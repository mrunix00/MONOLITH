/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./utils.h"
#include "./magic.h"

#include <libgfx.h>

void draw_box(gfx_context_t *context, gfx_rect_t rect)
{
    uint32_t border_cut = (BORDER_THICKNESS * 3) / 2;
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
    gfx_draw_line(
        context,
        (gfx_line_t) {
            .x1 = rect.x + BORDER_THICKNESS,
            .y1 = rect.y + BORDER_THICKNESS,
            .x2 = rect.x + rect.width - border_cut,
            .y2 = rect.y + BORDER_THICKNESS,
            .thickness = BORDER_SHADOW_THICKNESS,
        },
        BORDER_SHADOW_COLOR);
}
