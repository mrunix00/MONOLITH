/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <ipc.h>
#include <libdesktop.h>
#include <stdlib.h>
#include <string.h>

#define DESKTOP_IPC_RETRY_COUNT 5000
#define DESKTOP_EVENT_RECV_BUFFER_SIZE 4096
#define DESKTOP_MAX_FRAMEBUFFERS 64
#define DESKTOP_FRAMEBUFFER_MIN_CAPACITY 64

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct
{
    bool active;
    bool request_in_flight;
    uint16_t window_id;
    uint16_t view_width;
    uint16_t view_height;
    uint16_t buffer_width;
    uint16_t buffer_height;
    void *pixels;
    uint32_t *backbuffer;
    size_t backbuffer_size;
    void *stale_pixels;
    size_t size;
    uint32_t pending_sequence;
} _desktop_framebuffer_state_t;

static channel_id_t _protocol_channel_id = -1;
static uint32_t _sequence = 0;
static desktop_error_t _last_error = DESKTOP_ERROR_NONE;
static _desktop_framebuffer_state_t _framebuffer_states[DESKTOP_MAX_FRAMEBUFFERS];

static int _release_framebuffer_mapping(void *addr)
{
    if (!addr)
        return 0;
    if (_protocol_channel_id == -1)
        return -1;
    return ipc_release_shared_memory(_protocol_channel_id, addr);
}

static int _protocol_send_request(const desktop_request_t *request)
{
    if (!request) {
        _last_error = DESKTOP_ERROR_INVALID_ARGUMENT;
        return -1;
    }

    for (uint32_t attempt = 0; attempt < DESKTOP_IPC_RETRY_COUNT; attempt++) {
        if (_protocol_channel_id == -1) {
            if (desktop_connect() != 0) {
                usleep(1);
                continue;
            }
        }

        if (ipc_send(_protocol_channel_id, (void *) request, sizeof(*request)) == 0)
            return 0;

        _last_error = DESKTOP_ERROR_SEND_FAILED;
        usleep(1);
    }

    _last_error = DESKTOP_ERROR_SEND_FAILED;
    return -1;
}

static uint16_t _grow_capacity(uint16_t current, uint16_t required)
{
    if (required == 0)
        return 0;
    if (current >= required)
        return current;

    uint32_t grown = current ? current : DESKTOP_FRAMEBUFFER_MIN_CAPACITY;
    while (grown < required)
        grown *= 2;

    if (grown > UINT16_MAX)
        grown = UINT16_MAX;
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

static _desktop_framebuffer_state_t *_get_or_create_framebuffer_state(uint16_t window_id)
{
    _desktop_framebuffer_state_t *state = _find_framebuffer_state(window_id);
    if (state)
        return state;

    for (size_t i = 0; i < DESKTOP_MAX_FRAMEBUFFERS; i++) {
        if (_framebuffer_states[i].active)
            continue;

        memset(&_framebuffer_states[i], 0, sizeof(_framebuffer_states[i]));
        _framebuffer_states[i].active = true;
        _framebuffer_states[i].window_id = window_id;
        _framebuffer_states[i].pending_sequence = 0;
        return &_framebuffer_states[i];
    }

    return NULL;
}

static void _clear_framebuffer_state(_desktop_framebuffer_state_t *state)
{
    if (!state)
        return;

    _release_framebuffer_mapping(state->pixels);
    _release_framebuffer_mapping(state->stale_pixels);
    free(state->backbuffer);
    memset(state, 0, sizeof(*state));
}

static void _populate_context(const _desktop_framebuffer_state_t *state, gfx_context_t *out_context)
{
    if (!state || !out_context)
        return;

    memset(out_context, 0, sizeof(*out_context));
    out_context->framebuffer = (uint32_t *) state->pixels;
    out_context->backbuffer = state->backbuffer;
    out_context->width = state->buffer_width;
    out_context->height = state->buffer_height;
    out_context->pitch = (size_t) state->buffer_width * sizeof(uint32_t);
    out_context->bpp = 32;
    out_context->memory_model = 1;
    out_context->red_mask_size = 8;
    out_context->red_mask_shift = 16;
    out_context->green_mask_size = 8;
    out_context->green_mask_shift = 8;
    out_context->blue_mask_size = 8;
    out_context->blue_mask_shift = 0;

    uint16_t clip_w = MIN(state->view_width, state->buffer_width);
    uint16_t clip_h = MIN(state->view_height, state->buffer_height);
    out_context->clip_rect = (gfx_rect_t) {
        .x = 0,
        .y = 0,
        .width = clip_w,
        .height = clip_h,
        .border_color = {0, 0, 0, 0},
        .border_thickness = 0,
    };
}

static int _ensure_backbuffer(_desktop_framebuffer_state_t *state)
{
    if (!state)
        return -1;

    size_t backbuffer_size = (size_t) state->buffer_width * (size_t) state->buffer_height
                             * sizeof(uint32_t);
    if (backbuffer_size == 0)
        return 0;

    if (!state->backbuffer || state->backbuffer_size != backbuffer_size) {
        uint32_t *new_backbuffer = malloc(backbuffer_size);
        if (!new_backbuffer) {
            _last_error = DESKTOP_ERROR_OUT_OF_MEMORY;
            return -1;
        }

        free(state->backbuffer);
        state->backbuffer = new_backbuffer;
        state->backbuffer_size = backbuffer_size;
    }

    return 0;
}

static int _request_window_framebuffer_internal(
    _desktop_framebuffer_state_t *state,
    uint16_t requested_width,
    uint16_t requested_height,
    uint32_t sequence)
{
    if (!state || requested_width == 0 || requested_height == 0)
        return -1;

    size_t size = (size_t) requested_width * (size_t) requested_height * sizeof(uint32_t);
    if (size == 0)
        return -1;

    desktop_request_t request = {
        .sequence = sequence,
        .type = DESKTOP_REQUEST_REQUEST_FRAMEBUFFER,
        .data.request_framebuffer.id = state->window_id,
        .data.request_framebuffer.width = requested_width,
        .data.request_framebuffer.height = requested_height,
    };

    if (_protocol_send_request(&request) != 0)
        return -1;

    if (desktop_connect() != 0) {
        _last_error = DESKTOP_ERROR_CONNECT_FAILED;
        return -1;
    }

    void *new_pixels = NULL;
    if (ipc_request_shared_memory(_protocol_channel_id, size, IPC_SHM_FLAG_RW, &new_pixels) != 0) {
        _last_error = DESKTOP_ERROR_SHM_FAILED;
        return -1;
    }

    if (!new_pixels) {
        _last_error = DESKTOP_ERROR_SHM_FAILED;
        return -1;
    }

    uint32_t *old_pixels = (uint32_t *) state->pixels;
    uint16_t old_width = state->buffer_width;
    uint16_t old_height = state->buffer_height;
    uint16_t copy_width = 0;
    uint16_t copy_height = 0;

    if (old_pixels && old_width > 0 && old_height > 0) {
        copy_width = old_width < requested_width ? old_width : requested_width;
        copy_height = old_height < requested_height ? old_height : requested_height;
        for (uint16_t y = 0; y < copy_height; y++) {
            uint32_t *dst_row = (uint32_t *) new_pixels + (size_t) y * requested_width;
            uint32_t *src_row = old_pixels + (size_t) y * old_width;
            memcpy(dst_row, src_row, (size_t) copy_width * sizeof(uint32_t));
        }
    }

    for (uint16_t y = 0; y < copy_height; y++) {
        uint32_t *dst_row = (uint32_t *) new_pixels + (size_t) y * requested_width;
        if (requested_width > copy_width) {
            memset(
                dst_row + copy_width,
                0,
                (size_t) (requested_width - copy_width) * sizeof(uint32_t));
        }
    }

    if (requested_height > copy_height) {
        uint32_t *dst = (uint32_t *) new_pixels + (size_t) copy_height * requested_width;
        memset(
            dst,
            0,
            (size_t) (requested_height - copy_height) * requested_width * sizeof(uint32_t));
    }

    if (state->stale_pixels && state->stale_pixels != old_pixels)
        _release_framebuffer_mapping(state->stale_pixels);

    state->stale_pixels = old_pixels;

    state->pixels = new_pixels;
    state->size = size;
    state->buffer_width = requested_width;
    state->buffer_height = requested_height;
    state->pending_sequence = sequence;
    state->request_in_flight = true;

    return 0;
}

int desktop_connect(void)
{
    if (_protocol_channel_id != -1)
        return 0;

    _protocol_channel_id = ipc_connect(DESKTOP_CHANNEL_NAME);
    if (_protocol_channel_id == -1) {
        _last_error = DESKTOP_ERROR_CONNECT_FAILED;
        return -1;
    }

    return 0;
}

uint32_t desktop_create_window(
    uint16_t width, uint16_t height, window_flags_t flags, const char *title)
{
    _last_error = DESKTOP_ERROR_NONE;

    if (!title)
        title = "";

    uint32_t sequence = _sequence++;
    desktop_request_t request = {
        .sequence = sequence,
        .type = DESKTOP_REQUEST_CREATE_WINDOW,
        .data.create_window.width = width,
        .data.create_window.height = height,
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

    desktop_request_t request = {
        .sequence = _sequence++,
        .type = DESKTOP_REQUEST_PRESENT_WINDOW,
        .data.present_window.id = window_id,
    };

    return _protocol_send_request(&request);
}

gfx_context_res_t desktop_request_window_framebuffer(
    uint16_t window_id, uint16_t width, uint16_t height)
{
    gfx_context_res_t result;
    memset(&result, 0, sizeof(result));

    _last_error = DESKTOP_ERROR_NONE;

    result.sequence = _sequence++;

    _desktop_framebuffer_state_t *state = _get_or_create_framebuffer_state(window_id);
    if (!state) {
        _last_error = DESKTOP_ERROR_OUT_OF_MEMORY;
        return result;
    }

    state->view_width = width;
    state->view_height = height;

    if (state->pixels && !state->request_in_flight && state->buffer_width >= width
        && state->buffer_height >= height) {
        if (_ensure_backbuffer(state) != 0)
            return result;
        _populate_context(state, &result.context);
        return result;
    }

    if (state->request_in_flight) {
        if (_ensure_backbuffer(state) != 0)
            return result;
        _populate_context(state, &result.context);
        result.sequence = state->pending_sequence;
        return result;
    }

    uint16_t requested_width = _grow_capacity(state->buffer_width, width);
    uint16_t requested_height = _grow_capacity(state->buffer_height, height);
    if (_request_window_framebuffer_internal(state, requested_width, requested_height, result.sequence)
        != 0) {
        return result;
    }

    if (_ensure_backbuffer(state) != 0)
        return result;

    _populate_context(state, &result.context);
    return result;
}

desktop_error_t desktop_get_error(void)
{
    return _last_error;
}

int desktop_handle_framebuffer_event(const desktop_event_t *event, gfx_context_t *out_context)
{
    if (!event)
        return -1;

    if (event->type != DESKTOP_EVENT_FRAMEBUFFER_READY)
        return 0;

    _desktop_framebuffer_state_t *state = _find_framebuffer_state(event->data.framebuffer_ready.id);
    if (!state)
        return 0;

    if (!state->request_in_flight || state->pending_sequence != event->sequence)
        return 0;

    state->buffer_width = event->data.framebuffer_ready.width;
    state->buffer_height = event->data.framebuffer_ready.height;
    state->size = event->data.framebuffer_ready.size;
    state->request_in_flight = false;
    state->pending_sequence = 0;

    if (_ensure_backbuffer(state) != 0)
        return -1;

    if (state->stale_pixels && state->stale_pixels != state->pixels) {
        _release_framebuffer_mapping(state->stale_pixels);
        state->stale_pixels = NULL;
    }

    _populate_context(state, out_context);

    return 1;
}

int desktop_release_window_framebuffer(void *framebuffer_addr)
{
    if (!framebuffer_addr)
        return 0;

    if (desktop_connect() != 0)
        return -1;

    return ipc_release_shared_memory(_protocol_channel_id, framebuffer_addr);
}

int desktop_poll_event(desktop_event_t *event)
{
    if (!event)
        return -1;

    if (desktop_connect() != 0)
        return -1;

    unsigned char recv_buffer[DESKTOP_EVENT_RECV_BUFFER_SIZE];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    connection_t sender = {0};
    int result = ipc_receive(_protocol_channel_id, &sender, recv_buffer, sizeof(recv_buffer));
    if (result == 1)
        return 1;
    if (result != 0)
        return -1;

    memcpy(event, recv_buffer, sizeof(*event));

    return 0;
}

int desktop_disconnect(void)
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
