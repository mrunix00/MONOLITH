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
    DESKTOP_ERROR_ALLOC_FAILED = 5,
} desktop_error_t;

int desktop_connect();

uint32_t desktop_create_window(uint16_t w, uint16_t h, window_flags_t flags, const char *title);

uint32_t desktop_destroy_window(uint16_t window_id);

int desktop_present_window(uint16_t window_id);

int desktop_request_window_framebuffer(uint16_t window_id, uint16_t w, uint16_t h);

desktop_error_t desktop_get_error();

int desktop_handle_framebuffer_event(const desktop_event_t *event, gfx_context_t *out_context);

int desktop_release_window_framebuffer(void *framebuffer_addr);

int desktop_poll_event(desktop_event_t *event);

int desktop_disconnect();
