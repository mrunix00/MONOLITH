/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/input/input_events.h>
#include <kernel/klibc/memory.h>

#define INPUT_EVENT_RING_SIZE 128

static input_event_t _ring[INPUT_EVENT_RING_SIZE];
static volatile uint32_t _head = 0;
static volatile uint32_t _tail = 0;

static inline uint32_t _next_index(uint32_t index)
{
    return (index + 1) % INPUT_EVENT_RING_SIZE;
}

void input_events_init(void)
{
    _head = 0;
    _tail = 0;
    memset(_ring, 0, sizeof(_ring));
}

static input_event_t *_reserve_slot(void)
{
    uint32_t next = _next_index(_head);
    if (next == _tail) {
        _tail = _next_index(_tail);
    }
    input_event_t *slot = &_ring[_head];
    _head = next;
    return slot;
}

void input_push_keyboard_event(uint8_t scancode, uint8_t action)
{
    input_event_t *event = _reserve_slot();
    event->type = INPUT_EVENT_KEYBOARD;
    event->reserved = 0;
    event->reserved2 = 0;
    event->data.keyboard.scancode = scancode;
    event->data.keyboard.action = action;
}

void input_push_mouse_event(int32_t x, int32_t y, int8_t delta_x, int8_t delta_y, uint8_t buttons)
{
    input_event_t *event = _reserve_slot();
    event->type = INPUT_EVENT_MOUSE;
    event->reserved = 0;
    event->reserved2 = 0;
    event->data.mouse.x = x;
    event->data.mouse.y = y;
    event->data.mouse.delta_x = delta_x;
    event->data.mouse.delta_y = delta_y;
    event->data.mouse.buttons = buttons;
}

bool input_poll_event(input_event_t *event)
{
    if (!event)
        return false;
    if (_head == _tail)
        return false;
    memcpy(event, &_ring[_tail], sizeof(*event));
    _tail = _next_index(_tail);
    return true;
}
