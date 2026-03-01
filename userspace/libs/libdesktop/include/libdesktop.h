/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <protocol.h>
#include <stdint.h>

typedef enum {
    DESKTOP_ERROR_NONE = 0,
    DESKTOP_ERROR_INVALID_ARGUMENT = 1,
    DESKTOP_ERROR_CONNECT_FAILED = 2,
    DESKTOP_ERROR_SEND_FAILED = 3,
    DESKTOP_ERROR_SHM_FAILED = 4,
    DESKTOP_ERROR_OUT_OF_MEMORY = 5,
} desktop_error_t;

typedef struct
{
    uint32_t sequence;
    gfx_context_t context;
} gfx_context_res_t;

int desktop_connect(void);

uint32_t desktop_create_window(
    uint16_t width, uint16_t height, window_flags_t flags, const char *title);

uint32_t desktop_destroy_window(uint16_t window_id);

gfx_context_res_t desktop_request_window_framebuffer(
    uint16_t window_id, uint16_t width, uint16_t height);

desktop_error_t desktop_get_error(void);

int desktop_handle_framebuffer_event(const desktop_event_t *event, gfx_context_t *out_context);

int desktop_release_window_framebuffer(void *framebuffer_addr);

int desktop_poll_event(desktop_event_t *event);

int desktop_disconnect(void);
