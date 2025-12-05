/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include "types.h"
#include <stdbool.h>

/* Maximum number of windows */
#define MAX_WINDOWS 16

/* Window title bar height */
#define TITLEBAR_HEIGHT 20

/* Window button size */
#define WINDOW_BUTTON_SIZE 14
#define WINDOW_BUTTON_SPACING 2

/* Resize handle size */
#define RESIZE_BORDER 5
#define RESIZE_CORNER 12

/* Minimum window size */
#define MIN_WINDOW_WIDTH 100
#define MIN_WINDOW_HEIGHT 80

/* Window flags */
#define WINDOW_FLAG_CLOSABLE (1 << 0)
#define WINDOW_FLAG_DRAGGABLE (1 << 1)
#define WINDOW_FLAG_RESIZABLE (1 << 2)
#define WINDOW_FLAG_ACTIVE (1 << 3)
#define WINDOW_FLAG_MINIMIZABLE (1 << 4)
#define WINDOW_FLAG_MAXIMIZABLE (1 << 5)
#define WINDOW_FLAG_MINIMIZED (1 << 6)
#define WINDOW_FLAG_MAXIMIZED (1 << 7)
#define WINDOW_FLAG_SNAPPED_LEFT (1 << 8)
#define WINDOW_FLAG_SNAPPED_RIGHT (1 << 9)

/* Snap zone size (pixels from edge) */
#define SNAP_ZONE_SIZE 16

/* Resize edge flags */
typedef enum {
    RESIZE_NONE = 0,
    RESIZE_LEFT = (1 << 0),
    RESIZE_RIGHT = (1 << 1),
    RESIZE_TOP = (1 << 2),
    RESIZE_BOTTOM = (1 << 3)
} resize_edge_t;

/* Window structure */
typedef struct window
{
    int id;
    char title[64];
    rect_t bounds;       /* Full window bounds including title bar */
    rect_t content;      /* Content area (inside window) */
    rect_t saved_bounds; /* Saved bounds before maximize */
    uint32_t flags;
    bool visible;
    bool close_pressed;
    bool minimize_pressed;
    bool maximize_pressed;

    /* For dragging */
    bool dragging;
    int drag_offset_x;
    int drag_offset_y;

    /* For resizing */
    bool resizing;
    resize_edge_t resize_edge;
    int resize_start_x;
    int resize_start_y;
    rect_t resize_start_bounds;

    /* Callback for drawing content */
    void (*draw_callback)(struct window *win);
    void *user_data;
} window_t;

/* Initialize window manager */
void wm_init(void);

/* Create a new window */
window_t *wm_create_window(const char *title, int x, int y, int w, int h, uint32_t flags);

/* Destroy a window */
void wm_destroy_window(window_t *win);

/* Get window by ID */
window_t *wm_get_window(int id);

/* Bring window to front */
void wm_bring_to_front(window_t *win);

/* Get active (topmost) window */
window_t *wm_get_active(void);

/* Process input for windows - skip_click_handling should be true if click was consumed elsewhere */
void wm_process_input(bool skip_click_handling);

/* Draw all windows */
void wm_draw_all(void);

/* Set draw callback for window */
void wm_set_draw_callback(window_t *win, void (*callback)(window_t *));

/* Check if a point is inside a window */
bool wm_point_in_window(window_t *win, int x, int y);

/* Get window at point (topmost) */
window_t *wm_window_at_point(int x, int y);

/* Check if close button was clicked on any window, return the window */
window_t *wm_check_close_clicked(void);

/* Check if minimize button was clicked on any window, return the window */
window_t *wm_check_minimize_clicked(void);

/* Check if maximize button was clicked on any window, return the window */
window_t *wm_check_maximize_clicked(void);

/* Minimize a window */
void wm_minimize_window(window_t *win);

/* Restore a minimized window */
void wm_restore_window(window_t *win);

/* Maximize/restore a window */
void wm_toggle_maximize(window_t *win);

/* Get number of visible windows */
int wm_get_window_count(void);

/* Get window by index in creation order (stable order for taskbar) */
window_t *wm_get_window_by_creation_order(int index);

/* Get the resize edge being hovered over (for cursor changes) */
resize_edge_t wm_get_hover_resize_edge(void);

/* Check if any window is currently being resized */
bool wm_is_resizing(void);
