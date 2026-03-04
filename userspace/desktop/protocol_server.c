/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./protocol_server.h"
#include "./window.h"

#include <ipc.h>
#include <protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_PENDING_FB_REQUESTS 64
#define PROTOCOL_SERVER_RECV_BUFFER_SIZE 4096

typedef struct
{
    uint64_t task_id;
    uint32_t sequence;
    uint16_t window_id;
    uint16_t width;
    uint16_t height;
    size_t size;
    int active;
} pending_fb_request_t;

static channel_id_t _channel_id = -1;
static pending_fb_request_t _pending_fb[MAX_PENDING_FB_REQUESTS];

static int _send_event(uint64_t task_id, const desktop_event_t *event)
{
    if (_channel_id == -1 || !event || task_id == 0)
        return -1;

    connection_t connection = {.task_id = task_id};
    return ipc_send_to(_channel_id, &connection, (void *) event, sizeof(*event));
}

static void _send_error(uint64_t task_id, uint32_t sequence, uint32_t code, const char *message)
{
    desktop_event_t event = {
        .sequence = sequence,
        .type = DESKTOP_EVENT_ERROR,
        .data.error.error_code = code,
    };

    if (!message)
        message = "desktop error";

    size_t len = strnlen(message, sizeof(event.data.error.error_message) - 1);
    memcpy(event.data.error.error_message, message, len);
    event.data.error.error_message[len] = '\0';

    _send_event(task_id, &event);
}

static pending_fb_request_t *_reserve_pending_fb_request(void)
{
    for (size_t i = 0; i < MAX_PENDING_FB_REQUESTS; i++) {
        if (_pending_fb[i].active)
            continue;
        _pending_fb[i].active = 1;
        return &_pending_fb[i];
    }

    return NULL;
}

static pending_fb_request_t *_take_pending_fb_request(uint64_t task_id)
{
    for (size_t i = 0; i < MAX_PENDING_FB_REQUESTS; i++) {
        if (!_pending_fb[i].active || _pending_fb[i].task_id != task_id)
            continue;

        _pending_fb[i].active = 0;
        return &_pending_fb[i];
    }

    return NULL;
}

static void _drop_pending_fb_requests(uint64_t task_id)
{
    for (size_t i = 0; i < MAX_PENDING_FB_REQUESTS; i++) {
        if (_pending_fb[i].task_id != task_id)
            continue;
        _pending_fb[i].active = 0;
    }
}

static void _on_window_closed(window_t *window, void *user)
{
    (void) user;

    if (!window || window->owner_task_id == 0)
        return;

    desktop_event_t close_event = {
        .sequence = 0,
        .type = DESKTOP_EVENT_WINDOW_CLOSE,
        .data.close.id = (uint16_t) window->id,
    };
    _send_event(window->owner_task_id, &close_event);

    desktop_event_t closed_event = {
        .sequence = 0,
        .type = DESKTOP_EVENT_WINDOW_CLOSED,
        .data.closed.id = (uint16_t) window->id,
    };
    _send_event(window->owner_task_id, &closed_event);
}

static void _handle_create_window(uint64_t task_id, const desktop_request_t *request)
{
    uint32_t total_width = request->data.create_window.width;
    uint32_t total_height = request->data.create_window.height;

    window_t *window = new_window(
        request->data.create_window.title,
        total_width,
        total_height,
        request->data.create_window.flags);

    desktop_event_t event = {
        .sequence = request->sequence,
        .type = DESKTOP_EVENT_WINDOW_CREATED,
        .data.created.id = 0,
        .data.created.width = request->data.create_window.width,
        .data.created.height = request->data.create_window.height,
        .data.created.status = WINDOW_CREATED_ERROR,
    };

    if (!window) {
        _send_event(task_id, &event);
        return;
    }

    window->owner_task_id = task_id;
    window_set_close_callback(window, _on_window_closed, NULL);

    event.data.created.id = (uint16_t) window->id;
    event.data.created.width = window_get_content_width(window);
    event.data.created.height = window_get_content_height(window);
    window->notified_content_width = event.data.created.width;
    window->notified_content_height = event.data.created.height;
    event.data.created.status = WINDOW_CREATED_SUCCESS;
    _send_event(task_id, &event);
}

static void _handle_destroy_window(uint64_t task_id, const desktop_request_t *request)
{
    window_t *window = get_window_by_owner(task_id, request->data.close_window.id);
    if (!window) {
        _send_error(task_id, request->sequence, 2, "unknown window");
        return;
    }

    close_window_by_id(window->id);
}

static void _handle_request_framebuffer(uint64_t task_id, const desktop_request_t *request)
{
    window_t *window = get_window_by_owner(task_id, request->data.request_framebuffer.id);
    if (!window) {
        _send_error(task_id, request->sequence, 2, "unknown window");
        return;
    }

    pending_fb_request_t *pending = _reserve_pending_fb_request();
    if (!pending) {
        _send_error(task_id, request->sequence, 3, "framebuffer queue full");
        return;
    }

    pending->task_id = task_id;
    pending->sequence = request->sequence;
    pending->window_id = (uint16_t) window->id;
    uint16_t content_width = window_get_content_width(window);
    uint16_t content_height = window_get_content_height(window);
    uint16_t requested_width = request->data.request_framebuffer.width;
    uint16_t requested_height = request->data.request_framebuffer.height;

    pending->width = requested_width > content_width ? requested_width : content_width;
    pending->height = requested_height > content_height ? requested_height : content_height;
    pending->size = (size_t) pending->width * (size_t) pending->height * sizeof(uint32_t);
}

static void _handle_present_window(uint64_t task_id, const desktop_request_t *request)
{
    window_t *window = get_window_by_owner(task_id, request->data.present_window.id);
    if (!window) {
        _send_error(task_id, request->sequence, 2, "unknown window");
        return;
    }
}

static void _handle_client_request(uint64_t task_id, const desktop_request_t *request)
{
    if (!request)
        return;

    switch (request->type) {
    case DESKTOP_REQUEST_CREATE_WINDOW:
        _handle_create_window(task_id, request);
        break;
    case DESKTOP_REQUEST_CLOSE_WINDOW:
        _handle_destroy_window(task_id, request);
        break;
    case DESKTOP_REQUEST_REQUEST_FRAMEBUFFER:
        _handle_request_framebuffer(task_id, request);
        break;
    case DESKTOP_REQUEST_PRESENT_WINDOW:
        _handle_present_window(task_id, request);
        break;
    default:
        _send_error(task_id, request->sequence, 1, "unsupported request");
        break;
    }
}

static void _handle_shm_request(const ipc_message_t *message)
{
    if (!message)
        return;

    pending_fb_request_t *pending = _take_pending_fb_request(message->sender_task_id);
    if (!pending)
        return;

    window_t *window = get_window_by_owner(pending->task_id, pending->window_id);
    if (!window)
        return;

    size_t shm_size = message->payload.shm_request.size;
    uint16_t width = pending->width;
    uint16_t height = pending->height;

    if (shm_size == 0 || width == 0) {
        _send_error(pending->task_id, pending->sequence, 4, "invalid framebuffer shm");
        return;
    }

    size_t max_pixels = shm_size / sizeof(uint32_t);
    if (max_pixels == 0) {
        _send_error(pending->task_id, pending->sequence, 4, "invalid framebuffer shm");
        return;
    }

    size_t max_rows = max_pixels / width;
    if (max_rows == 0) {
        width = (uint16_t) (max_pixels > UINT16_MAX ? UINT16_MAX : max_pixels);
        if (width == 0) {
            _send_error(pending->task_id, pending->sequence, 4, "invalid framebuffer shm");
            return;
        }
        max_rows = max_pixels / width;
        if (max_rows == 0)
            max_rows = 1;
    }

    if ((size_t) height > max_rows)
        height = (uint16_t) (max_rows > UINT16_MAX ? UINT16_MAX : max_rows);

    size_t effective_size = (size_t) width * (size_t) height * sizeof(uint32_t);
    if (effective_size > shm_size)
        effective_size = shm_size;

    window_set_remote_surface(
        window,
        (uint32_t *) message->payload.shm_request.owner_address,
        width,
        height,
        effective_size);

    desktop_event_t ready_event = {
        .sequence = pending->sequence,
        .type = DESKTOP_EVENT_FRAMEBUFFER_READY,
        .data.framebuffer_ready.id = pending->window_id,
        .data.framebuffer_ready.width = width,
        .data.framebuffer_ready.height = height,
        .data.framebuffer_ready.stride = (uint32_t) width * sizeof(uint32_t),
        .data.framebuffer_ready.size = effective_size,
    };
    _send_event(pending->task_id, &ready_event);
}

static void _handle_disconnect(uint64_t task_id)
{
    _drop_pending_fb_requests(task_id);
    close_windows_by_owner(task_id);
}

static bool _pump_window_resize_events(void)
{
    bool had_resize_event = false;
    size_t window_count = window_get_count();
    for (size_t i = 0; i < window_count; i++) {
        window_t *window = window_get_at_index(i);
        if (!window || window->owner_task_id == 0)
            continue;

        uint16_t content_width = window_get_content_width(window);
        uint16_t content_height = window_get_content_height(window);
        if (content_width == 0 || content_height == 0)
            continue;

        if (window->notified_content_width == 0 || window->notified_content_height == 0) {
            window->notified_content_width = content_width;
            window->notified_content_height = content_height;
            continue;
        }

        if (window->notified_content_width == content_width
            && window->notified_content_height == content_height) {
            continue;
        }

        desktop_event_t resize_event = {
            .sequence = 0,
            .type = DESKTOP_EVENT_WINDOW_RESIZED,
            .data.resized.id = (uint16_t) window->id,
            .data.resized.new_width = content_width,
            .data.resized.new_height = content_height,
        };
        _send_event(window->owner_task_id, &resize_event);
        had_resize_event = true;

        window->notified_content_width = content_width;
        window->notified_content_height = content_height;
    }

    return had_resize_event;
}

static void _accept_pending_connections(void)
{
    connection_t connection;
    int result;

    while ((result = ipc_await_connection(_channel_id, &connection)) == 0)
        ipc_accept_connection(_channel_id, &connection);
}

int protocol_server_init(void)
{
    if (_channel_id != -1)
        return 0;

    _channel_id = ipc_new_channel(DESKTOP_CHANNEL_NAME);
    if (_channel_id == -1)
        return -1;

    memset(_pending_fb, 0, sizeof(_pending_fb));
    return 0;
}

bool protocol_server_pump(void)
{
    if (_channel_id == -1)
        return false;

    bool had_activity = false;

    _accept_pending_connections();

    unsigned char raw_message[PROTOCOL_SERVER_RECV_BUFFER_SIZE] = {0};
    connection_t sender = {0};

    while (ipc_receive(_channel_id, &sender, raw_message, sizeof(raw_message)) == 0) {
        had_activity = true;
        ipc_message_t *message = (ipc_message_t *) raw_message;

        switch (message->type) {
        case IPC_OWNER_MSG_TYPE_DATA:
            if (message->payload.data.size < sizeof(desktop_request_t)) {
                _send_error(message->sender_task_id, 0, 4, "invalid request size");
                break;
            }
            _handle_client_request(
                message->sender_task_id, (const desktop_request_t *) message->payload.data.data);
            break;
        case IPC_OWNER_MSG_TYPE_SHM_REQUEST:
            _handle_shm_request(message);
            break;
        case IPC_OWNER_MSG_TYPE_DISCONNECT:
            _handle_disconnect(message->sender_task_id);
            break;
        default:
            break;
        }
    }

    if (_pump_window_resize_events())
        had_activity = true;

    return had_activity;
}
