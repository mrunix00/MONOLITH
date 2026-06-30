/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/memory.h>
#include <stdlib.h>

#define DEFAULT_ARENA_CAPACITY 4096

static ui_arena_node_t *_ui_arena_new_node(size_t capacity)
{
    ui_arena_node_t *node = malloc(sizeof(ui_arena_node_t));
    if (node == NULL)
        return NULL;

    node->data = malloc(capacity);
    if (node->data == NULL)
        return free(node), NULL;
    node->next = NULL;
    node->capacity = capacity;
    node->size = 0;
    return node;
}

static void _ui_arena_init(ui_arena_t *arena, size_t capacity)
{
    ui_arena_node_t *node = _ui_arena_new_node(capacity);
    if (node == NULL)
        return;

    arena->head = node;
    arena->tail = node;
    arena->total_capacity = capacity;
}

void ui_arena_init(ui_arena_t *arena)
{
    _ui_arena_init(arena, DEFAULT_ARENA_CAPACITY);
}

void ui_arena_free(ui_arena_t *arena)
{
    ui_arena_node_t *node = arena->head;
    while (node != NULL) {
        ui_arena_node_t *next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    arena->head = NULL;
    arena->total_capacity = 0;
}

void *ui_arena_alloc(ui_arena_t *arena, size_t size)
{
    if (arena->tail != NULL && arena->tail->size + size <= arena->tail->capacity) {
        void *ptr = arena->tail->data + arena->tail->size;
        arena->tail->size += size;
        return ptr;
    }

    ui_arena_node_t *node = _ui_arena_new_node(DEFAULT_ARENA_CAPACITY);
    if (node == NULL)
        return NULL;

    arena->tail->next = node;
    arena->tail = node;
    arena->total_capacity += DEFAULT_ARENA_CAPACITY;
    node->size = size;
    return node->data;
}

/* Merge all existing nodes and reset the arena */
void ui_arena_reset(ui_arena_t *arena)
{
    if (arena->head == arena->tail) {
        arena->head->size = 0;
        return;
    }
    size_t total_capacity = arena->total_capacity;
    ui_arena_free(arena);
    _ui_arena_init(arena, total_capacity);
}
