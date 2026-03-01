/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DESKTOP_CHANNEL_NAME "desktop"
#define DESKTOP_TITLE_MAX 64

typedef enum {
    DESKTOP_REQUEST_CREATE_WINDOW = 0,
    DESKTOP_REQUEST_CLOSE_WINDOW,
    DESKTOP_REQUEST_REQUEST_FRAMEBUFFER,
} desktop_request_type_t;

typedef struct
{
    bool borderless : 1;
    bool fullscreen : 1;
    bool resizable : 1;
    bool maximized : 1;
    bool minimized : 1;
    uint8_t padding : 3;
} window_flags_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    window_flags_t flags;
    char title[DESKTOP_TITLE_MAX];
} create_window_req_t;

typedef struct
{
    uint16_t id;
} close_window_req_t;

typedef struct
{
    uint16_t id;
    uint16_t width;
    uint16_t height;
} request_framebuffer_req_t;

typedef struct
{
    uint32_t sequence;
    desktop_request_type_t type;
    union {
        create_window_req_t create_window;
        close_window_req_t close_window;
        request_framebuffer_req_t request_framebuffer;
    } data;
} desktop_request_t;

typedef enum {
    DESKTOP_EVENT_ERROR = 0,
    DESKTOP_EVENT_WINDOW_CREATED,
    DESKTOP_EVENT_WINDOW_CLOSE,
    DESKTOP_EVENT_WINDOW_CLOSED,
    DESKTOP_EVENT_WINDOW_RESIZED,
    DESKTOP_EVENT_FRAMEBUFFER_READY,
} desktop_event_type_t;

typedef struct
{
    uint32_t error_code;
    char error_message[64];
} desktop_error_event_t;

typedef struct
{
    uint16_t id;
    uint16_t width;
    uint16_t height;
    enum {
        WINDOW_CREATED_ERROR = 0,
        WINDOW_CREATED_SUCCESS,
    } status;
} window_created_event_t;

typedef struct
{
    uint16_t id;
} window_close_event_t;

typedef struct
{
    uint16_t id;
} window_closed_event_t;

typedef struct
{
    uint16_t id;
    uint16_t new_width;
    uint16_t new_height;
} window_resized_event_t;

typedef struct
{
    uint16_t id;
    uint16_t width;
    uint16_t height;
    uint32_t stride;
    size_t size;
} window_framebuffer_ready_event_t;

typedef struct
{
    uint32_t sequence;
    desktop_event_type_t type;
    union {
        desktop_error_event_t error;
        window_created_event_t created;
        window_close_event_t close;
        window_closed_event_t closed;
        window_resized_event_t resized;
        window_framebuffer_ready_event_t framebuffer_ready;
    } data;
} desktop_event_t;
