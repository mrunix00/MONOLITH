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
    uint32_t x;
    uint32_t y;
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

void menu_draw(gfx_context_t *context, const menu_t *menu);
int32_t menu_hit_test(const menu_t *menu, uint32_t x, uint32_t y);
bool menu_contains_point(const menu_t *menu, uint32_t x, uint32_t y);

void menubar_draw(gfx_context_t *context, const menubar_t *bar);
void menubar_draw_open_menus(gfx_context_t *context, const menubar_t *bar);
void menubar_handle_click(menubar_t *bar, uint32_t x, uint32_t y);
void menubar_close_all(menubar_t *bar);

void update_menubar_state(gfx_context_t *context);
void draw_menubar(gfx_context_t *context);
menubar_t *menubar_state_get_active(void);
