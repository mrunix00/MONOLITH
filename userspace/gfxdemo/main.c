/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libdesktop.h>
#include <libgfx.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct
{
    float x;
    float y;
    float z;
} vec3_t;

typedef struct
{
    uint32_t a;
    uint32_t b;
} edge_t;

static float _absf(float x)
{
    return x < 0.0f ? -x : x;
}

static float _wrap_radians(float x)
{
    const float pi = 3.14159265358979323846f;
    const float two_pi = 6.28318530717958647692f;

    while (x > pi)
        x -= two_pi;
    while (x < -pi)
        x += two_pi;

    return x;
}

static float _fast_sin(float x)
{
    x = _wrap_radians(x);

    float y = 1.27323954f * x - 0.405284735f * x * _absf(x);
    y = 0.225f * (y * _absf(y) - y) + y;
    return y;
}

static float _fast_cos(float x)
{
    return _fast_sin(x + 1.57079632679489661923f);
}

static vec3_t _rotate_vertex(vec3_t v, float ax, float ay, float az)
{
    float sx = _fast_sin(ax);
    float cx = _fast_cos(ax);
    float sy = _fast_sin(ay);
    float cy = _fast_cos(ay);
    float sz = _fast_sin(az);
    float cz = _fast_cos(az);

    vec3_t rx = {
        .x = v.x,
        .y = v.y * cx - v.z * sx,
        .z = v.y * sx + v.z * cx,
    };

    vec3_t ry = {
        .x = rx.x * cy + rx.z * sy,
        .y = rx.y,
        .z = -rx.x * sy + rx.z * cy,
    };

    vec3_t rz = {
        .x = ry.x * cz - ry.y * sz,
        .y = ry.x * sz + ry.y * cz,
        .z = ry.z,
    };

    return rz;
}

static void _draw_cube_frame(gfx_context_t *ctx, uint32_t target_w, uint32_t target_h, float angle)
{
    if (!ctx || !ctx->framebuffer || !ctx->backbuffer)
        return;

    uint32_t draw_w = target_w;
    uint32_t draw_h = target_h;

    if (draw_w > ctx->width)
        draw_w = (uint32_t) ctx->width;
    if (draw_h > ctx->height)
        draw_h = (uint32_t) ctx->height;

    if (draw_w < 16 || draw_h < 16)
        return;

    static const vec3_t cube_vertices[8] = {
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    };

    static const edge_t cube_edges[12] = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    gfx_begin_frame(ctx);

    gfx_draw_filled_rect(
        ctx, (gfx_rect_t){0, 0, draw_w, draw_h, {0}, 0}, (gfx_color_t){0xFF, 0x10, 0x14, 0x1E});

    gfx_set_clip(ctx, (gfx_area_t){0, 0, draw_w, draw_h});

    uint32_t projected_x[8] = {0};
    uint32_t projected_y[8] = {0};

    float center_x = (float) draw_w * 0.5f;
    float center_y = (float) draw_h * 0.5f;
    float scale = (float) (draw_w < draw_h ? draw_w : draw_h) * 0.42f;

    for (uint32_t i = 0; i < 8; i++) {
        vec3_t r = _rotate_vertex(cube_vertices[i], angle * 0.7f, angle, angle * 0.45f);

        float z = r.z + 4.0f;
        if (z < 0.2f)
            z = 0.2f;

        float inv_z = 1.0f / z;
        float px = center_x + r.x * scale * inv_z;
        float py = center_y + r.y * scale * inv_z;

        if (px < 0.0f)
            px = 0.0f;
        if (py < 0.0f)
            py = 0.0f;
        if (px > (float) (draw_w - 1))
            px = (float) (draw_w - 1);
        if (py > (float) (draw_h - 1))
            py = (float) (draw_h - 1);

        projected_x[i] = (uint32_t) px;
        projected_y[i] = (uint32_t) py;
    }

    for (uint32_t i = 0; i < 12; i++) {
        uint32_t a = cube_edges[i].a;
        uint32_t b = cube_edges[i].b;

        gfx_draw_line(
            ctx,
            (gfx_line_t){
                .x1 = projected_x[a],
                .y1 = projected_y[a],
                .x2 = projected_x[b],
                .y2 = projected_y[b],
                .thickness = 2,
            },
            (gfx_color_t){0xFF, 0x71, 0xD0, 0xFF});
    }

    gfx_end_frame(ctx);
}

int main(void)
{
    bool created = false;
    bool create_pending = false;
    bool framebuffer_ready = false;

    uint32_t create_sequence = 0;
    uint32_t window_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    float angle = 0.0f;
    gfx_context_t framebuffer = {0};

    if (desktop_connect() != 0)
        exit();

    while (1) {
        if (!created && !create_pending) {
            create_sequence
                = desktop_create_window(560, 420, (window_flags_t){.resizable = true}, "GFX Demo");
            create_pending = true;
        }

        desktop_event_t event;
        int poll_result = desktop_poll_event(&event);

        if (poll_result == 0) {
            if (event.type == DESKTOP_EVENT_WINDOW_CREATED && create_pending
                && event.sequence == create_sequence) {
                if (event.data.created.status != WINDOW_CREATED_SUCCESS) {
                    create_pending = false;
                    usleep(1);
                    continue;
                }

                created = true;
                create_pending = false;
                framebuffer_ready = false;
                window_id = event.data.created.window_id;
                width = event.data.created.width;
                height = event.data.created.height;
                desktop_request_window_framebuffer(window_id, width, height);
                continue;
            }

            if (event.type == DESKTOP_EVENT_FRAMEBUFFER_READY
                && event.data.framebuffer_ready.id == window_id
                && desktop_handle_framebuffer_event(&event, &framebuffer) == 1) {
                gfx_set_target_fps(&framebuffer, 60);
                framebuffer_ready = true;
                continue;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_RESIZED
                && event.data.resized.window_id == window_id) {
                width = event.data.resized.new_width;
                height = event.data.resized.new_height;
                framebuffer_ready = desktop_request_window_framebuffer(window_id, width, height)
                                    == 1;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_CLOSE
                && event.data.close.window_id == window_id) {
                desktop_destroy_window((uint16_t) window_id);
                framebuffer_ready = false;
                continue;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_CLOSED
                && event.data.closed.window_id == window_id)
                break;
        }

        if (created && framebuffer_ready) {
            _draw_cube_frame(&framebuffer, width, height, angle);
            desktop_present_window((uint16_t) window_id);
            angle += 0.03f;
            if (angle > 6.28318530717958647692f)
                angle -= 6.28318530717958647692f;
        } else {
            usleep(1);
        }
    }

    exit();
}
