/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */
#pragma once

#include <resource.h>
#include <stdint.h>

#define TERM_RD_TIN 0
#define TERM_RD_TOUT 1

#define TERM_MAX_PAYLOAD 4096

typedef enum {
    TERM_EVENT_INVALID = 0,
    TERM_EVENT_INPUT_VT100,
    TERM_EVENT_WINDOW_RESIZED,
    TERM_EVENT_WINDOW_INFO,
} term_event_type_t;

typedef struct
{
    term_event_type_t type;
    uint32_t length;
} term_event_t;

typedef struct
{
    uint16_t rows;
    uint16_t cols;
} term_dimensions_t;

typedef enum {
    TERM_COMMAND_INVALID = 0,
    TERM_COMMAND_WRITE_VT100,
    TERM_COMMAND_GET_TERM_INFO,
} term_command_type_t;

typedef struct
{
    term_command_type_t type;
    uint32_t length;
} term_command_t;

int term_write_command(
    rsrc_handle_t rd, term_command_type_t type, const void *payload, uint32_t length);
int term_write_event(rsrc_handle_t rd, term_event_type_t type, const void *payload, uint32_t length);
int term_read_command(rsrc_handle_t rd, term_command_t *cmd, void *payload, uint32_t length);
int term_read_event(rsrc_handle_t rd, term_event_t *event, void *payload, uint32_t length);
