/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>

/* Taskbar height */
#define TASKBAR_HEIGHT 28

/* Desktop icon size */
#define ICON_WIDTH 32
#define ICON_HEIGHT 32
#define ICON_SPACING 20
#define ICON_TEXT_OFFSET 4

/* Icon grid layout constants */
#define ICON_GRID_START_X 20
#define ICON_GRID_START_Y (TASKBAR_HEIGHT + 20)
#define ICON_GRID_CELL_WIDTH (ICON_WIDTH + 48)
#define ICON_GRID_CELL_HEIGHT (ICON_HEIGHT + 40)

/* Maximum menu items */
#define MAX_MENU_ITEMS 16
#define MAX_CONTEXT_MENU_ITEMS 16

/* Context menu item structure */
typedef struct
{
    char label[32];
    bool separator;
    bool enabled;
    bool checked;      /* Checkmark indicator */
    char shortcut[16]; /* Keyboard shortcut text (e.g., "Ctrl+V") */
    void (*callback)(void);
} context_menu_item_t;

/* Context menu structure */
typedef struct
{
    context_menu_item_t items[MAX_CONTEXT_MENU_ITEMS];
    int num_items;
    int x, y; /* Position where menu appears */
    bool visible;
    int hover_index; /* Currently hovered item, -1 if none */
} context_menu_t;

/* Cursor types */
typedef enum {
    CURSOR_ARROW,
    CURSOR_RESIZE_NS,   /* North-South (vertical resize) */
    CURSOR_RESIZE_EW,   /* East-West (horizontal resize) */
    CURSOR_RESIZE_NWSE, /* Northwest-Southeast (diagonal) */
    CURSOR_RESIZE_NESW, /* Northeast-Southwest (diagonal) */
    CURSOR_MOVE,        /* Move cursor (4-way arrows) */
    CURSOR_COUNT
} cursor_type_t;

/* Menu item structure */
typedef struct
{
    char label[32];
    bool separator;
    bool enabled;
    void (*callback)(void);
} menu_item_t;

/* Menu structure */
typedef struct
{
    menu_item_t items[MAX_MENU_ITEMS];
    int num_items;
    bool open;
} menu_t;

/* Desktop icon structure */
typedef struct
{
    char label[32];
    int x, y;
    bool selected;
} desktop_icon_t;

/* Initialize desktop */
void desktop_init(void);

/* Add an item to the menu */
void desktop_add_menu_item(const char *label, void (*callback)(void));

/* Add a separator to the menu */
void desktop_add_menu_separator(void);

/* Add an icon to the desktop at next available grid position */
void desktop_add_icon_grid(const char *label);

/* Delete an icon by index */
void desktop_delete_icon(int index);

/* Get the index of the currently selected icon (-1 if none) */
int desktop_get_selected_icon(void);

/* Get the index of the icon at a given position (-1 if none) */
int desktop_get_icon_at(int x, int y);

/* Process desktop input - returns true if click was consumed */
bool desktop_process_input(void);

/* Draw the desktop */
void desktop_draw(void);

/* Get taskbar height */
int desktop_get_taskbar_height(void);

/* Check if menu is open */
bool desktop_menu_open(void);

/* Close all menus */
void desktop_close_menus(void);

/* Draw the mouse cursor */
void desktop_draw_cursor(void);

/* Set the current cursor type */
void desktop_set_cursor(cursor_type_t cursor);

/* Get the current cursor type */
cursor_type_t desktop_get_cursor(void);

/* Context menu functions */

/* Show context menu at specified position */
void desktop_show_context_menu(int x, int y);

/* Add an item to the context menu */
void desktop_add_context_menu_item(const char *label, const char *shortcut, void (*callback)(void));

/* Add a checked item to the context menu */
void desktop_add_context_menu_item_checked(
    const char *label, const char *shortcut, bool checked, void (*callback)(void));

/* Add a disabled item to the context menu */
void desktop_add_context_menu_item_disabled(const char *label, const char *shortcut);

/* Add a separator to the context menu */
void desktop_add_context_menu_separator(void);

/* Clear all items from context menu */
void desktop_clear_context_menu(void);

/* Close/hide the context menu */
void desktop_close_context_menu(void);

/* Check if context menu is currently visible */
bool desktop_context_menu_visible(void);

/* Set callback for building context menu dynamically (called on right-click) */
typedef void (*context_menu_builder_t)(void);
void desktop_set_context_menu_builder(context_menu_builder_t builder);
