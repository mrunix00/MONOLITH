/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <ipc.h>
#include <libdesktop.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DESKTOP_EVENT_RECV_BUFFER_SIZE 4096
#define DESKTOP_MAX_FRAMEBUFFERS 64

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct
{
    bool active;
    uint16_t window_id;
    uint16_t view_width;
    uint16_t view_height;
    uint16_t buffer_width;
    uint16_t buffer_height;
    void *framebuffer;
    uint32_t *backbuffer;
    size_t backbuffer_size;
    gfx_context_t *bound_context;
} _desktop_framebuffer_state_t;

static channel_id_t _protocol_channel_id = -1;
static uint32_t _sequence = 0;
static desktop_error_t _last_error = DESKTOP_ERROR_NONE;
static _desktop_framebuffer_state_t _framebuffer_states[DESKTOP_MAX_FRAMEBUFFERS];

static int _protocol_send_request(const desktop_request_t *request)
{
    if (ipc_send(_protocol_channel_id, (void *) request, sizeof(*request)) == 0)
        return 0;
    _last_error = DESKTOP_ERROR_SEND_FAILED;
    return -1;
}

static uint16_t _grow_capacity(uint16_t current, uint16_t required)
{
    if (required == 0)
        return 0;
    if (current == 0)
        return required;

    uint32_t grown = (uint32_t) current * 2;
    if (grown < required)
        grown = required;
    if (grown > UINT16_MAX)
        return UINT16_MAX;
    return (uint16_t) grown;
}

static _desktop_framebuffer_state_t *_find_framebuffer_state(uint16_t window_id)
{
    for (size_t i = 0; i < DESKTOP_MAX_FRAMEBUFFERS; i++) {
        if (_framebuffer_states[i].active && _framebuffer_states[i].window_id == window_id)
            return &_framebuffer_states[i];
    }

    return NULL;
}

static void _clear_framebuffer_state(_desktop_framebuffer_state_t *state)
{
    ipc_release_shared_memory(_protocol_channel_id, state->framebuffer);
    free(state->backbuffer);
    memset(state, 0, sizeof(*state));
}

static void _populate_context(const _desktop_framebuffer_state_t *state, gfx_context_t *out_context)
{
    if (!out_context)
        return;

    uint32_t target_fps = out_context->target_fps;
    memset(out_context, 0, sizeof(*out_context));

    out_context->framebuffer = (uint32_t *) state->framebuffer;
    out_context->backbuffer = state->backbuffer;
    out_context->width = state->view_width;
    out_context->height = state->view_height;
    out_context->pitch = (size_t) state->buffer_width * sizeof(uint32_t);
    out_context->bpp = 32;
    out_context->memory_model = 1;
    out_context->red_mask_size = 8;
    out_context->red_mask_shift = 16;
    out_context->green_mask_size = 8;
    out_context->green_mask_shift = 8;
    out_context->blue_mask_size = 8;
    out_context->blue_mask_shift = 0;
    out_context->target_fps = target_fps;
    out_context->clip_rect
        = (gfx_area_t){.x = 0, .y = 0, .width = state->view_width, .height = state->view_height};
}

static int _ensure_backbuffer(_desktop_framebuffer_state_t *state)
{
    size_t backbuffer_size = (size_t) state->buffer_width * (size_t) state->buffer_height
                             * sizeof(uint32_t);
    if (backbuffer_size == 0)
        return 0;

    if (!state->backbuffer || state->backbuffer_size < backbuffer_size) {
        uint32_t *new_backbuffer = malloc(backbuffer_size);
        if (!new_backbuffer) {
            _last_error = DESKTOP_ERROR_ALLOC_FAILED;
            return -1;
        }

        free(state->backbuffer);
        state->backbuffer = new_backbuffer;
        state->backbuffer_size = backbuffer_size;
    }

    return 0;
}

static int _request_window_framebuffer(
    uint16_t window_id, uint16_t width, uint16_t height, uint32_t sequence)
{
    desktop_request_t request = {
        .sequence = sequence,
        .type = DESKTOP_REQUEST_REQUEST_FRAMEBUFFER,
        .data.request_framebuffer.id = window_id,
        .data.request_framebuffer.width = width,
        .data.request_framebuffer.height = height,
    };

    if (_protocol_send_request(&request) != 0)
        return -1;

    return 0;
}

int desktop_connect(void)
{
    _protocol_channel_id = ipc_connect(DESKTOP_CHANNEL_NAME);
    if (_protocol_channel_id == -1) {
        _last_error = DESKTOP_ERROR_CONNECT_FAILED;
        return -1;
    }

    connection_t connection = {0};
    if (ipc_await_connection(_protocol_channel_id, &connection) != 0) {
        _last_error = DESKTOP_ERROR_CONNECT_FAILED;
        return -1;
    }

    return 0;
}

uint32_t desktop_create_window(uint16_t w, uint16_t h, window_flags_t flags, const char *title)
{
    _last_error = DESKTOP_ERROR_NONE;

    uint32_t sequence = _sequence++;
    desktop_request_t request = {
        .sequence = sequence,
        .type = DESKTOP_REQUEST_CREATE_WINDOW,
        .data.create_window.width = w,
        .data.create_window.height = h,
        .data.create_window.flags = flags,
    };
    size_t title_len = strnlen(title, sizeof(request.data.create_window.title) - 1);
    memcpy(request.data.create_window.title, title, title_len);
    request.data.create_window.title[title_len] = '\0';

    if (_protocol_send_request(&request) != 0)
        return sequence;

    return sequence;
}

uint32_t desktop_destroy_window(uint16_t window_id)
{
    _last_error = DESKTOP_ERROR_NONE;

    _desktop_framebuffer_state_t *state = _find_framebuffer_state(window_id);
    if (state)
        _clear_framebuffer_state(state);

    uint32_t sequence = _sequence++;
    desktop_request_t request = {
        .sequence = sequence,
        .type = DESKTOP_REQUEST_CLOSE_WINDOW,
        .data.close_window.id = window_id,
    };

    if (_protocol_send_request(&request) != 0)
        return sequence;

    return sequence;
}

int desktop_present_window(uint16_t window_id)
{
    _last_error = DESKTOP_ERROR_NONE;

    _desktop_framebuffer_state_t *state = _find_framebuffer_state(window_id);
    desktop_request_t request = {
        .sequence = _sequence++,
        .type = DESKTOP_REQUEST_PRESENT_WINDOW,
        .data.present_window.id = window_id,
        .data.present_window.width = state ? state->view_width : 0,
        .data.present_window.height = state ? state->view_height : 0,
    };

    return _protocol_send_request(&request);
}

static _desktop_framebuffer_state_t *new_framebuffer_state(uint16_t window_id)
{
    for (size_t i = 0; i < DESKTOP_MAX_FRAMEBUFFERS; i++) {
        if (_framebuffer_states[i].active)
            continue;
        _framebuffer_states[i] = (_desktop_framebuffer_state_t){0};
        _framebuffer_states[i].active = true;
        _framebuffer_states[i].window_id = window_id;
        return &_framebuffer_states[i];
    }
    return NULL;
}

int desktop_request_window_framebuffer(uint16_t window_id, uint16_t w, uint16_t h)
{
    _last_error = DESKTOP_ERROR_NONE;

    _desktop_framebuffer_state_t *state = _find_framebuffer_state(window_id);
    if (state == NULL) {
        state = new_framebuffer_state(window_id);
        if (state == NULL) {
            _last_error = DESKTOP_ERROR_ALLOC_FAILED;
            return -1;
        }
    }

    state->view_width = w;
    state->view_height = h;

    if (state->framebuffer && state->buffer_width >= w && state->buffer_height >= h) {
        if (!state->backbuffer) {
            _last_error = DESKTOP_ERROR_ALLOC_FAILED;
            return -1;
        }
        if (state->bound_context)
            _populate_context(state, state->bound_context);
        desktop_present_window(window_id);
        return 1;
    }

    uint16_t requested_width = _grow_capacity(state->buffer_width, w);
    uint16_t requested_height = _grow_capacity(state->buffer_height, h);
    if (_request_window_framebuffer(window_id, requested_width, requested_height, _sequence++) != 0)
        return -1;

    return 0;
}

desktop_error_t desktop_get_error(void)
{
    return _last_error;
}

int desktop_handle_framebuffer_event(const desktop_event_t *event, gfx_context_t *out_context)
{
    if (event->type != DESKTOP_EVENT_FRAMEBUFFER_READY)
        return 0;

    _desktop_framebuffer_state_t *state = _find_framebuffer_state(event->data.framebuffer_ready.id);
    if (!state)
        return 0;

    void *new_pixels = (void *) event->data.framebuffer_ready.address;
    if (!new_pixels) {
        _last_error = DESKTOP_ERROR_SHM_FAILED;
        return -1;
    }

    ipc_release_shared_memory(_protocol_channel_id, state->framebuffer);
    state->framebuffer = new_pixels;
    state->buffer_width = event->data.framebuffer_ready.width;
    state->buffer_height = event->data.framebuffer_ready.height;
    if (state->view_width == 0 || state->view_width > state->buffer_width)
        state->view_width = state->buffer_width;
    if (state->view_height == 0 || state->view_height > state->buffer_height)
        state->view_height = state->buffer_height;

    if (_ensure_backbuffer(state) != 0)
        return -1;

    state->bound_context = out_context;
    _populate_context(state, out_context);
    desktop_present_window(state->window_id);

    return 1;
}

int desktop_release_window_framebuffer(void *framebuffer_addr)
{
    if (!framebuffer_addr)
        return 0;

    return ipc_release_shared_memory(_protocol_channel_id, framebuffer_addr);
}

int desktop_poll_event(desktop_event_t *event)
{
    unsigned char recv_buffer[DESKTOP_EVENT_RECV_BUFFER_SIZE];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    connection_t sender = {0};
    int result = ipc_receive(_protocol_channel_id, &sender, recv_buffer, sizeof(recv_buffer));
    if (result == 0)
        return 1;
    if (result < 0)
        return -1;

    memcpy(event, recv_buffer, sizeof(*event));

    return 0;
}

int desktop_disconnect()
{
    for (size_t i = 0; i < DESKTOP_MAX_FRAMEBUFFERS; i++) {
        if (!_framebuffer_states[i].active)
            continue;
        _clear_framebuffer_state(&_framebuffer_states[i]);
    }

    if (_protocol_channel_id == -1)
        return 0;

    int result = ipc_disconnect(_protocol_channel_id);
    _protocol_channel_id = -1;
    return result;
}
