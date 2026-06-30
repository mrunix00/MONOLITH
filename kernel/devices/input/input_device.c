/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/device_domain.h>
#include <kernel/devices/input/input_device.h>
#include <kernel/devices/input/ps2_keyboard.h>
#include <kernel/devices/input/ps2_mouse.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>

#define INPUT_RING_SIZE 128

/*
 * Keyboard ring buffer.
 */
static input_keyboard_event_t _kb_ring[INPUT_RING_SIZE];
static volatile uint32_t _kb_head = 0;
static volatile uint32_t _kb_tail = 0;

/*
 * Mouse ring buffer.
 */
static input_mouse_event_t _mouse_ring[INPUT_RING_SIZE];
static volatile uint32_t _mouse_head = 0;
static volatile uint32_t _mouse_tail = 0;

static inline uint32_t _next_index(uint32_t index)
{
    return (index + 1) % INPUT_RING_SIZE;
}

void input_push_keyboard_event(uint8_t scancode, uint8_t action)
{
    uint32_t next = _next_index(_kb_head);
    if (next == _kb_tail)
        _kb_tail = _next_index(_kb_tail);

    _kb_ring[_kb_head] = (input_keyboard_event_t){.scancode = scancode, .action = action};
    _kb_head = next;
}

void input_push_mouse_event(int32_t x, int32_t y, int8_t delta_x, int8_t delta_y, uint8_t buttons)
{
    uint32_t next = _next_index(_mouse_head);
    if (next == _mouse_tail)
        _mouse_tail = _next_index(_mouse_tail);

    _mouse_ring[_mouse_head] = (input_mouse_event_t){
        .x = x, .y = y, .delta_x = delta_x, .delta_y = delta_y, .buttons = buttons};
    _mouse_head = next;
}

bool input_poll_keyboard_event(void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (_kb_head == _kb_tail)
        return false;

    uint64_t copy_size = sizeof(input_keyboard_event_t);
    if (buffer_len < copy_size)
        copy_size = buffer_len;

    memcpy(buffer, &_kb_ring[_kb_tail], copy_size);
    _kb_tail = _next_index(_kb_tail);
    *out_bytes_read = copy_size;
    return true;
}

bool input_poll_mouse_event(void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (_mouse_head == _mouse_tail)
        return false;

    uint64_t copy_size = sizeof(input_mouse_event_t);
    if (buffer_len < copy_size)
        copy_size = buffer_len;

    memcpy(buffer, &_mouse_ring[_mouse_tail], copy_size);
    _mouse_tail = _next_index(_mouse_tail);
    *out_bytes_read = copy_size;
    return true;
}

static rsrc_status_t _input_device_read(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    (void) handle_state;

    if (buffer == NULL || out_bytes_read == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    bool (*poll_fn)(void *, uint64_t, uint64_t *);
    if (strcmp(resource->header.name, "keyboard") == 0)
        poll_fn = input_poll_keyboard_event;
    else if (strcmp(resource->header.name, "mouse") == 0)
        poll_fn = input_poll_mouse_event;
    else
        return RSRC_ERROR_NOT_SUPPORTED;

    if (!poll_fn(buffer, buffer_len, out_bytes_read))
        *out_bytes_read = 0;

    return RSRC_STATUS_OK;
}

static rsrc_status_t _input_device_poll(
    rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events)
{
    (void) handle_state;

    if (resource == NULL || out_ready_events == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    uint64_t ready = 0;
    if ((requested_events & RSRC_POLL_READ) != 0) {
        if (strcmp(resource->header.name, "keyboard") == 0) {
            if (_kb_head != _kb_tail)
                ready |= RSRC_POLL_READ;
        } else if (strcmp(resource->header.name, "mouse") == 0) {
            if (_mouse_head != _mouse_tail)
                ready |= RSRC_POLL_READ;
        } else {
            return RSRC_ERROR_NOT_SUPPORTED;
        }
    }

    *out_ready_events = ready;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _input_device_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = NULL,
    .list = NULL,
    .read = _input_device_read,
    .write = NULL,
    .mmap = NULL,
    .poll = _input_device_poll,
    .remove = NULL,
    .control = NULL,
};

void input_devices_init(void)
{
    ps2_init_keyboard();
    ps2_mouse_init();
    device_register("/input/keyboard", &_input_device_ops, NULL);
    device_register("/input/mouse", &_input_device_ops, NULL);
}
