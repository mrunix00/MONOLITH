/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx/fonts.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "./profiler.h"

typedef struct
{
    int frame_count;
    int calls_in_frame;
    uint64_t frame_duration;
    uint64_t total_frame_duration;
    uint64_t last_sample;
} _prof_data_t;

static _prof_data_t _data[256] = {0};

extern gfx_font_t default_font;

void prof_start(int id)
{
    _data[id].last_sample = get_ticks();
}

void prof_end(int id)
{
    uint64_t end_sample = get_ticks();
    uint64_t duration = end_sample - _data[id].last_sample;
    _data[id].calls_in_frame++;
    _data[id].frame_duration += duration;
}

void draw_perf_stats(gfx_context_t *context)
{
    char buffer[32];
    int pos_y = 45;

    for (int i = 0; i < 256; i++) {
        if (_data[i].calls_in_frame > 0) {
            _data[i].total_frame_duration += _data[i].frame_duration;
            _data[i].frame_count++;
            _data[i].frame_duration = 0;
            _data[i].calls_in_frame = 0;
        }

        if (_data[i].frame_count == 0)
            continue;

        sprintf(
            buffer,
            "ID: %d, Avg: %dms",
            i,
            (int) (_data[i].total_frame_duration / (uint64_t) _data[i].frame_count));
        gfx_draw_text(
            context,
            &default_font,
            (gfx_pos_t) {.x = 0, .y = pos_y},
            (gfx_color_t) {.r = 255, .g = 255, .b = 255, .a = 255},
            buffer);
        pos_y += 15;
    }
}
