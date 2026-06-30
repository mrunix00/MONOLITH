/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <input_events.h>
#include <resource.h>

static inline rsrc_handle_t open_keyboard_device(void)
{
    return rsmgr_open("device:/input/keyboard");
}

static inline rsrc_handle_t open_mouse_device(void)
{
    return rsmgr_open("device:/input/mouse");
}

static inline int read_keyboard_event(rsrc_handle_t fd, input_keyboard_event_t *event)
{
    uint64_t bytes_read = 0;
    int result = rsmgr_read(fd, event, sizeof(input_keyboard_event_t), &bytes_read);
    return result < 0 ? result : (int) bytes_read;
}

static inline int read_mouse_event(rsrc_handle_t fd, input_mouse_event_t *event)
{
    uint64_t bytes_read = 0;
    int result = rsmgr_read(fd, event, sizeof(input_mouse_event_t), &bytes_read);
    return result < 0 ? result : (int) bytes_read;
}
