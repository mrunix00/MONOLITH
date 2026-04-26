/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*menu_action_t)(void);

typedef enum {
    MENU_ITEM_ACTION,
    MENU_ITEM_SEPARATOR,
} menu_item_type_t;

typedef struct
{
    const char *label;
    menu_item_type_t type;
    menu_action_t action;
} menu_item_t;

typedef struct
{
    const menu_item_t *items;
    size_t item_count;
    bool open;
} menu_t;

typedef struct
{
    const char *label;
    menu_t *menu;
} menubar_item_t;

typedef struct
{
    menubar_item_t *items;
    size_t item_count;
} menubar_t;

bool update_menubar_state(gfx_context_t *context);
void draw_menubar(gfx_context_t *context);
