/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>

typedef struct _ui_arena_node ui_arena_node_t;
struct _ui_arena_node
{
    void *data;
    size_t size;
    size_t capacity;
    ui_arena_node_t *next;
};

typedef struct
{
    ui_arena_node_t *head;
    ui_arena_node_t *tail;
    size_t total_capacity;
} ui_arena_t;

void ui_arena_init(ui_arena_t *arena);
void ui_arena_free(ui_arena_t *arena);
void *ui_arena_alloc(ui_arena_t *arena, size_t size);
void ui_arena_reset(ui_arena_t *arena);
