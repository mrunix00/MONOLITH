/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <protocol.h>
#include <stdbool.h>
#include <stdint.h>

#include "./window.h"

int protocol_server_init();

bool protocol_server_pump();

int protocol_send_event(uint64_t task_id, desktop_event_t event);

void protocol_send_error(uint64_t task_id, uint32_t sequence, uint32_t code, const char *message);

void protocol_release_window_surface(window_t *window);
