/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "./protocol_server.h"
#include "./input.h"
#include "./window.h"

#include <ipc.h>
#include <string.h>

#define PROTOCOL_SERVER_RECV_BUFFER_SIZE 4096
#define MAX_PENDING_SURFACE_RELEASES 64

typedef struct
{
    uint64_t task_id;
    void *pixels;
    size_t size;
} _pending_surface_release_t;

static channel_id_t _channel_id = -1;
static _pending_surface_release_t _pending_releases[MAX_PENDING_SURFACE_RELEASES];
static size_t _pending_releases_count = 0;

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

static size_t _pages_for_size(size_t size)
{
    if (size == 0)
        return 0;
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static void _defer_surface_release(uint64_t task_id, void *pixels, size_t size)
{
    if (!pixels || size == 0)
        return;

    if (_pending_releases_count >= MAX_PENDING_SURFACE_RELEASES) {
        /* Table full -- release immediately as a last resort. */
        if (ipc_release_shared_memory(_channel_id, pixels) != 0) {
            size_t pages = _pages_for_size(size);
            unmap_pages(pixels, pages);
        }
        return;
    }

    _pending_surface_release_t *entry = &_pending_releases[_pending_releases_count++];
    entry->task_id = task_id;
    entry->pixels = pixels;
    entry->size = size;
}

static void _flush_pending_releases(uint64_t task_id)
{
    size_t i = 0;
    while (i < _pending_releases_count) {
        if (_pending_releases[i].task_id != task_id) {
            i++;
            continue;
        }

        _pending_surface_release_t entry = _pending_releases[i];
        size_t pages = _pages_for_size(entry.size);
        unmap_pages(entry.pixels, pages);

        _pending_releases[i] = _pending_releases[--_pending_releases_count];
    }
}

static void _on_window_closed(window_t *window)
{
    if (!window || window->owner_task_id == 0)
        return;

    /* Don't release the surface here -- the client may still be mid-render
     * (preempted by the timer). Defer release until the client disconnects. */
    if (window->surface_pixels && window->surface_size > 0)
        _defer_surface_release(window->owner_task_id, window->surface_pixels, window->surface_size);
    window_set_remote_surface(window, NULL, 0, 0, 0);

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
    window_set_close_callback(window, _on_window_closed);

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

    uint16_t content_width = window_get_content_width(window);
    uint16_t content_height = window_get_content_height(window);
    uint16_t requested_width = request->data.request_framebuffer.width;
    uint16_t requested_height = request->data.request_framebuffer.height;

    uint16_t width = requested_width > content_width ? requested_width : content_width;
    uint16_t height = requested_height > content_height ? requested_height : content_height;
    size_t effective_size = (size_t) width * (size_t) height * sizeof(uint32_t);
    size_t pages = _pages_for_size(effective_size);

    if (width == 0 || height == 0 || pages == 0) {
        _send_error(task_id, request->sequence, 4, "invalid framebuffer size");
        return;
    }

    uint32_t *old_pixels = window->surface_pixels;
    uint16_t old_width = window->surface_width;
    uint16_t old_height = window->surface_height;

    void *owner_pixels = alloc_pages(pages, ALLOC_PAGES_FLAG_RW);
    if (!owner_pixels) {
        _send_error(task_id, request->sequence, 5, "framebuffer allocation failed");
        return;
    }

    memset(owner_pixels, 0, pages * PAGE_SIZE);
    if (old_pixels && old_width > 0 && old_height > 0) {
        uint16_t copy_width = old_width < width ? old_width : width;
        uint16_t copy_height = old_height < height ? old_height : height;
        for (uint16_t y = 0; y < copy_height; y++) {
            memcpy(
                (uint32_t *) owner_pixels + ((size_t) y * width),
                old_pixels + ((size_t) y * old_width),
                (size_t) copy_width * sizeof(uint32_t));
        }
    }

    connection_t client = {
        .task_id = task_id,
    };
    void *client_pixels = ipc_share_memory(_channel_id, &client, owner_pixels, pages * PAGE_SIZE);
    if (!client_pixels) {
        unmap_pages(owner_pixels, pages);
        _send_error(task_id, request->sequence, 4, "framebuffer share failed");
        return;
    }

    window_set_remote_surface(window, (uint32_t *) owner_pixels, width, height, effective_size);

    desktop_event_t ready_event = {
        .sequence = request->sequence,
        .type = DESKTOP_EVENT_FRAMEBUFFER_READY,
        .data.framebuffer_ready.id = (uint16_t) window->id,
        .data.framebuffer_ready.address = (uintptr_t) client_pixels,
        .data.framebuffer_ready.width = width,
        .data.framebuffer_ready.height = height,
        .data.framebuffer_ready.stride = (uint32_t) width * sizeof(uint32_t),
        .data.framebuffer_ready.size = effective_size,
    };
    _send_event(task_id, &ready_event);
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

static void _handle_disconnect(uint64_t task_id)
{
    close_windows_by_owner(task_id);
    _flush_pending_releases(task_id);
}

static bool _pump_input_events(void)
{
    bool had_input = false;
    input_event_t input_event = {0};

    while (poll_input_event(&input_event) == 0) {
        had_input = true;
        input_process_event(&input_event);

        window_t *window = get_active_window();
        if (!window || window->owner_task_id == 0)
            continue;

        desktop_event_t event = {
            .sequence = 0,
        };

        if (input_event.type == INPUT_EVENT_KEYBOARD) {
            event.type = DESKTOP_EVENT_WINDOW_KEYBOARD;
            event.data.keyboard.id = (uint16_t) window->id;
            event.data.keyboard.keyboard = input_event.data.keyboard;
            _send_event(window->owner_task_id, &event);
            continue;
        }

        if (input_event.type == INPUT_EVENT_MOUSE) {
            input_mouse_event_t mouse_state = get_mouse_state();
            uint32_t mouse_x = mouse_state.x < 0 ? 0u : (uint32_t) mouse_state.x;
            uint32_t mouse_y = mouse_state.y < 0 ? 0u : (uint32_t) mouse_state.y;
            if (!window_contains_content_point(window, mouse_x, mouse_y))
                continue;

            event.type = DESKTOP_EVENT_WINDOW_MOUSE;
            event.data.mouse.id = (uint16_t) window->id;
            event.data.mouse.mouse = input_event.data.mouse;
            _send_event(window->owner_task_id, &event);
        }
    }

    return had_input;
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

    while ((result = ipc_poll_connection(_channel_id, &connection)) == 0)
        ipc_accept_connection(_channel_id, &connection);
}

int protocol_server_init(void)
{
    if (_channel_id != -1)
        return 0;

    _channel_id = ipc_new_channel(DESKTOP_CHANNEL_NAME);
    if (_channel_id == -1)
        return -1;

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

    while (ipc_receive(_channel_id, &sender, raw_message, sizeof(raw_message)) > 0) {
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
        case IPC_OWNER_MSG_TYPE_DISCONNECT:
            _handle_disconnect(message->sender_task_id);
            break;
        default:
            break;
        }
    }

    if (_pump_window_resize_events())
        had_activity = true;

    if (_pump_input_events())
        had_activity = true;

    return had_activity;
}
