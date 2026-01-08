/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "window.h"
#include "font.h"
#include "input.h"
#include "types.h"
#include <libgfx.h>
#include <stdlib.h>
#include <string.h>

/* Window storage - dynamically allocated */
static window_t *windows = NULL;
static int *window_order = NULL; /* Index order for z-ordering (last = topmost) */
static int num_windows = 0;
static int windows_capacity = 0;
static int next_window_id = 1;

/* Hover state for cursor changes */
static resize_edge_t hover_resize_edge = RESIZE_NONE;

void wm_init(void)
{
    num_windows = 0;
    windows_capacity = 0;
    next_window_id = 1;
}

/* Initial capacity for window arrays */
#define INITIAL_WINDOWS_CAPACITY 8

/* Grow the window arrays if needed */
static bool wm_grow_arrays(void)
{
    int new_capacity = windows_capacity == 0 ? INITIAL_WINDOWS_CAPACITY : windows_capacity * 2;

    /* realloc(NULL, size) behaves like malloc(size) */
    window_t *new_windows = realloc(windows, new_capacity * sizeof(window_t));
    if (!new_windows)
        return false;

    int *new_order = realloc(window_order, new_capacity * sizeof(int));
    if (!new_order)
        return false;

    /* Initialize new slots */
    for (int i = windows_capacity; i < new_capacity; i++) {
        memset(&new_windows[i], 0, sizeof(window_t));
        new_order[i] = -1;
    }

    windows = new_windows;
    window_order = new_order;
    windows_capacity = new_capacity;

    return true;
}

window_t *wm_create_window(const char *title, int x, int y, int w, int h, uint32_t flags)
{
    if (num_windows >= windows_capacity) {
        if (!wm_grow_arrays())
            return NULL;
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < windows_capacity; i++) {
        if (!windows[i].visible && windows[i].id == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        /* No free slot found, try to grow again */
        if (!wm_grow_arrays())
            return NULL;

        slot = windows_capacity - INITIAL_WINDOWS_CAPACITY; /* First slot in new allocation */
        /* Find actual free slot */
        for (int i = 0; i < windows_capacity; i++) {
            if (!windows[i].visible && windows[i].id == 0) {
                slot = i;
                break;
            }
        }
    }

    window_t *win = &windows[slot];

    win->id = next_window_id++;
    strncpy(win->title, title, 63);
    win->title[63] = '\0';

    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;

    win->content.x = x + 4;
    win->content.y = y + TITLEBAR_HEIGHT + 3;
    win->content.w = w - 8;
    win->content.h = h - TITLEBAR_HEIGHT - 7;

    win->flags = flags | WINDOW_FLAG_ACTIVE;
    win->visible = true;
    win->close_pressed = false;
    win->minimize_pressed = false;
    win->maximize_pressed = false;
    win->dragging = false;
    win->resizing = false;
    win->resize_edge = RESIZE_NONE;
    win->draw_callback = NULL;
    win->user_data = NULL;
    win->saved_bounds = win->bounds;

    /* Add to window order */
    window_order[num_windows] = slot;
    num_windows++;

    /* Deactivate other windows */
    for (int i = 0; i < windows_capacity; i++) {
        if (&windows[i] != win && windows[i].visible) {
            windows[i].flags &= ~WINDOW_FLAG_ACTIVE;
        }
    }

    return win;
}

void wm_destroy_window(window_t *win)
{
    if (!win || !win->visible || !windows || !window_order)
        return;

    /* Find in order array */
    int idx = -1;
    for (int i = 0; i < num_windows; i++) {
        if (window_order[i] >= 0 && &windows[window_order[i]] == win) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        /* Remove from order */
        for (int i = idx; i < num_windows - 1; i++) {
            window_order[i] = window_order[i + 1];
        }
        window_order[num_windows - 1] = -1;
        num_windows--;
    }

    /* Clear window */
    memset(win, 0, sizeof(window_t));

    /* Activate topmost window */
    if (num_windows > 0) {
        int top = window_order[num_windows - 1];
        if (top >= 0) {
            windows[top].flags |= WINDOW_FLAG_ACTIVE;
        }
    }
}

window_t *wm_get_window(int id)
{
    if (!windows)
        return NULL;

    for (int i = 0; i < windows_capacity; i++) {
        if (windows[i].id == id && windows[i].visible) {
            return &windows[i];
        }
    }
    return NULL;
}

void wm_bring_to_front(window_t *win)
{
    if (!win || !win->visible || !windows || !window_order)
        return;

    /* Find window slot */
    int slot = -1;
    for (int i = 0; i < windows_capacity; i++) {
        if (&windows[i] == win) {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return;

    /* Find position in order */
    int idx = -1;
    for (int i = 0; i < num_windows; i++) {
        if (window_order[i] == slot) {
            idx = i;
            break;
        }
    }

    if (idx < 0 || idx == num_windows - 1)
        return; /* Already at top or not found */

    /* Move to end */
    for (int i = idx; i < num_windows - 1; i++) {
        window_order[i] = window_order[i + 1];
    }
    window_order[num_windows - 1] = slot;

    /* Update active state */
    for (int i = 0; i < windows_capacity; i++) {
        if (windows[i].visible) {
            windows[i].flags &= ~WINDOW_FLAG_ACTIVE;
        }
    }
    win->flags |= WINDOW_FLAG_ACTIVE;
}

window_t *wm_get_active(void)
{
    if (num_windows <= 0 || !windows || !window_order)
        return NULL;
    int top = window_order[num_windows - 1];
    if (top >= 0) {
        return &windows[top];
    }
    return NULL;
}

bool wm_point_in_window(window_t *win, int x, int y)
{
    if (!win || !win->visible)
        return false;
    return x >= win->bounds.x && x < win->bounds.x + win->bounds.w && y >= win->bounds.y
           && y < win->bounds.y + win->bounds.h;
}

window_t *wm_window_at_point(int x, int y)
{
    if (!windows || !window_order)
        return NULL;

    /* Check from top to bottom (reverse order) */
    for (int i = num_windows - 1; i >= 0; i--) {
        int slot = window_order[i];
        if (slot >= 0 && windows[slot].visible && !(windows[slot].flags & WINDOW_FLAG_MINIMIZED)) {
            if (wm_point_in_window(&windows[slot], x, y)) {
                return &windows[slot];
            }
        }
    }
    return NULL;
}

static bool point_in_close_button(window_t *win, int x, int y)
{
    if (!(win->flags & WINDOW_FLAG_CLOSABLE))
        return false;

    /* Close button is leftmost */
    int bx = win->bounds.x + 4;
    int by = win->bounds.y + (TITLEBAR_HEIGHT - WINDOW_BUTTON_SIZE) / 2;

    return x >= bx && x < bx + WINDOW_BUTTON_SIZE && y >= by && y < by + WINDOW_BUTTON_SIZE;
}

static bool point_in_maximize_button(window_t *win, int x, int y)
{
    if (!(win->flags & WINDOW_FLAG_MAXIMIZABLE))
        return false;

    /* Maximize button is second from left (after close) */
    int num_buttons = 0;
    if (win->flags & WINDOW_FLAG_CLOSABLE)
        num_buttons++;

    int bx = win->bounds.x + 4 + num_buttons * (WINDOW_BUTTON_SIZE + WINDOW_BUTTON_SPACING);
    int by = win->bounds.y + (TITLEBAR_HEIGHT - WINDOW_BUTTON_SIZE) / 2;

    return x >= bx && x < bx + WINDOW_BUTTON_SIZE && y >= by && y < by + WINDOW_BUTTON_SIZE;
}

static bool point_in_minimize_button(window_t *win, int x, int y)
{
    if (!(win->flags & WINDOW_FLAG_MINIMIZABLE))
        return false;

    /* Minimize button is rightmost of the left-side buttons */
    int num_buttons = 0;
    if (win->flags & WINDOW_FLAG_CLOSABLE)
        num_buttons++;
    if (win->flags & WINDOW_FLAG_MAXIMIZABLE)
        num_buttons++;

    int bx = win->bounds.x + 4 + num_buttons * (WINDOW_BUTTON_SIZE + WINDOW_BUTTON_SPACING);
    int by = win->bounds.y + (TITLEBAR_HEIGHT - WINDOW_BUTTON_SIZE) / 2;

    return x >= bx && x < bx + WINDOW_BUTTON_SIZE && y >= by && y < by + WINDOW_BUTTON_SIZE;
}

static bool point_in_titlebar(window_t *win, int x, int y)
{
    return x >= win->bounds.x && x < win->bounds.x + win->bounds.w && y >= win->bounds.y
           && y < win->bounds.y + TITLEBAR_HEIGHT;
}

static resize_edge_t get_resize_edge(window_t *win, int x, int y)
{
    if (!(win->flags & WINDOW_FLAG_RESIZABLE))
        return RESIZE_NONE;

    resize_edge_t edge = RESIZE_NONE;

    int bx = win->bounds.x;
    int by = win->bounds.y;
    int bw = win->bounds.w;
    int bh = win->bounds.h;

    /* Check corners first (they take priority) */
    /* Bottom-right corner */
    if (x >= bx + bw - RESIZE_CORNER && x < bx + bw && y >= by + bh - RESIZE_CORNER && y < by + bh) {
        return RESIZE_RIGHT | RESIZE_BOTTOM;
    }
    /* Bottom-left corner */
    if (x >= bx && x < bx + RESIZE_CORNER && y >= by + bh - RESIZE_CORNER && y < by + bh) {
        return RESIZE_LEFT | RESIZE_BOTTOM;
    }
    /* Top-right corner */
    if (x >= bx + bw - RESIZE_CORNER && x < bx + bw && y >= by && y < by + RESIZE_CORNER) {
        return RESIZE_RIGHT | RESIZE_TOP;
    }
    /* Top-left corner */
    if (x >= bx && x < bx + RESIZE_CORNER && y >= by && y < by + RESIZE_CORNER) {
        return RESIZE_LEFT | RESIZE_TOP;
    }

    /* Check edges */
    /* Left edge */
    if (x >= bx && x < bx + RESIZE_BORDER) {
        edge |= RESIZE_LEFT;
    }
    /* Right edge */
    if (x >= bx + bw - RESIZE_BORDER && x < bx + bw) {
        edge |= RESIZE_RIGHT;
    }
    /* Top edge */
    if (y >= by && y < by + RESIZE_BORDER) {
        edge |= RESIZE_TOP;
    }
    /* Bottom edge */
    if (y >= by + bh - RESIZE_BORDER && y < by + bh) {
        edge |= RESIZE_BOTTOM;
    }

    return edge;
}

void wm_process_input(bool skip_click_handling)
{
    if (!windows || !window_order)
        return;

    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool left = input_mouse_left();
    bool clicked = skip_click_handling ? false : input_mouse_left_clicked();

    /* Reset hover state */
    hover_resize_edge = RESIZE_NONE;

    /* Check for resizing first */
    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->resizing) {
            if (left) {
                /* Calculate new bounds based on resize edge */
                int dx = mx - win->resize_start_x;
                int dy = my - win->resize_start_y;

                int new_x = win->resize_start_bounds.x;
                int new_y = win->resize_start_bounds.y;
                int new_w = win->resize_start_bounds.w;
                int new_h = win->resize_start_bounds.h;

                if (win->resize_edge & RESIZE_LEFT) {
                    new_x = win->resize_start_bounds.x + dx;
                    new_w = win->resize_start_bounds.w - dx;
                }
                if (win->resize_edge & RESIZE_RIGHT) {
                    new_w = win->resize_start_bounds.w + dx;
                }
                if (win->resize_edge & RESIZE_TOP) {
                    new_y = win->resize_start_bounds.y + dy;
                    new_h = win->resize_start_bounds.h - dy;
                }
                if (win->resize_edge & RESIZE_BOTTOM) {
                    new_h = win->resize_start_bounds.h + dy;
                }

                /* Enforce minimum size */
                if (new_w < MIN_WINDOW_WIDTH) {
                    if (win->resize_edge & RESIZE_LEFT) {
                        new_x = win->resize_start_bounds.x + win->resize_start_bounds.w
                                - MIN_WINDOW_WIDTH;
                    }
                    new_w = MIN_WINDOW_WIDTH;
                }
                if (new_h < MIN_WINDOW_HEIGHT) {
                    if (win->resize_edge & RESIZE_TOP) {
                        new_y = win->resize_start_bounds.y + win->resize_start_bounds.h
                                - MIN_WINDOW_HEIGHT;
                    }
                    new_h = MIN_WINDOW_HEIGHT;
                }

                /* Apply new bounds */
                win->bounds.x = new_x;
                win->bounds.y = new_y;
                win->bounds.w = new_w;
                win->bounds.h = new_h;

                /* Update content rect */
                win->content.x = win->bounds.x + 4;
                win->content.y = win->bounds.y + TITLEBAR_HEIGHT + 3;
                win->content.w = win->bounds.w - 8;
                win->content.h = win->bounds.h - TITLEBAR_HEIGHT - 7;

                /* Keep the resize cursor while resizing */
                hover_resize_edge = win->resize_edge;
            } else {
                /* Stop resizing */
                win->resizing = false;
                win->resize_edge = RESIZE_NONE;
            }
            return;
        }
    }

    /* Check for dragging */
    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->dragging) {
            if (left) {
                gfx_context_t *fb = &g_ctx;
                int screen_w = (int) fb->width;
                int screen_h = (int) fb->height;
                int taskbar_h = 28; /* TASKBAR_HEIGHT */

                /* Check snap zones based on mouse position while dragging */
                if (my <= taskbar_h + SNAP_ZONE_SIZE) {
                    /* Snap to top = maximize */
                    win->bounds.x = 0;
                    win->bounds.y = taskbar_h;
                    win->bounds.w = screen_w;
                    win->bounds.h = screen_h - taskbar_h;
                    win->flags |= WINDOW_FLAG_MAXIMIZED;
                    win->flags &= ~(WINDOW_FLAG_SNAPPED_LEFT | WINDOW_FLAG_SNAPPED_RIGHT);
                } else if (mx <= SNAP_ZONE_SIZE) {
                    /* Snap to left half */
                    win->bounds.x = 0;
                    win->bounds.y = taskbar_h;
                    win->bounds.w = screen_w / 2;
                    win->bounds.h = screen_h - taskbar_h;
                    win->flags |= WINDOW_FLAG_SNAPPED_LEFT;
                    win->flags &= ~(WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_RIGHT);
                } else if (mx >= screen_w - SNAP_ZONE_SIZE) {
                    /* Snap to right half */
                    win->bounds.x = screen_w / 2;
                    win->bounds.y = taskbar_h;
                    win->bounds.w = screen_w / 2;
                    win->bounds.h = screen_h - taskbar_h;
                    win->flags |= WINDOW_FLAG_SNAPPED_RIGHT;
                    win->flags &= ~(WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT);
                } else {
                    /* Not in snap zone - restore original size and follow mouse */
                    if (win->flags
                        & (WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT
                           | WINDOW_FLAG_SNAPPED_RIGHT)) {
                        win->bounds.w = win->resize_start_bounds.w;
                        win->bounds.h = win->resize_start_bounds.h;
                        win->flags &= ~(
                            WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT
                            | WINDOW_FLAG_SNAPPED_RIGHT);
                    }
                    /* Update position following mouse */
                    win->bounds.x = mx - win->drag_offset_x;
                    win->bounds.y = my - win->drag_offset_y;
                }

                /* Update content rect */
                win->content.x = win->bounds.x + 4;
                win->content.y = win->bounds.y + TITLEBAR_HEIGHT + 3;
                win->content.w = win->bounds.w - 8;
                win->content.h = win->bounds.h - TITLEBAR_HEIGHT - 7;
            } else {
                /* Stop dragging - save bounds if snapped */
                win->dragging = false;
                if (win->flags
                    & (WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT
                       | WINDOW_FLAG_SNAPPED_RIGHT)) {
                    win->saved_bounds = win->resize_start_bounds;
                }
            }
            return;
        }
    }

    /* Check for new interactions */
    if (clicked) {
        window_t *win = wm_window_at_point(mx, my);
        if (win) {
            /* Bring to front */
            wm_bring_to_front(win);

            /* Check close button */
            if (point_in_close_button(win, mx, my)) {
                win->close_pressed = true;
            }
            /* Check maximize button */
            else if (point_in_maximize_button(win, mx, my)) {
                win->maximize_pressed = true;
            }
            /* Check minimize button */
            else if (point_in_minimize_button(win, mx, my)) {
                win->minimize_pressed = true;
            }
            /* Check for resize edge */
            else {
                resize_edge_t edge = get_resize_edge(win, mx, my);
                if (edge != RESIZE_NONE) {
                    win->resizing = true;
                    win->resize_edge = edge;
                    win->resize_start_x = mx;
                    win->resize_start_y = my;
                    win->resize_start_bounds = win->bounds;
                }
                /* Check titlebar for dragging */
                else if (point_in_titlebar(win, mx, my) && (win->flags & WINDOW_FLAG_DRAGGABLE)) {
                    win->dragging = true;
                    win->drag_offset_x = mx - win->bounds.x;
                    win->drag_offset_y = my - win->bounds.y;
                    /* Save current bounds for snap - use saved_bounds if already snapped */
                    if (win->flags
                        & (WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT
                           | WINDOW_FLAG_SNAPPED_RIGHT)) {
                        /* When dragging from snapped/maximized, adjust drag offset to center of saved width */
                        win->drag_offset_x = win->saved_bounds.w / 2;
                        /* resize_start_bounds already has original bounds from saved_bounds */
                        win->resize_start_bounds = win->saved_bounds;
                    } else {
                        /* Save bounds for potential snap */
                        win->resize_start_bounds = win->bounds;
                    }
                }
            }
        }
    }

    /* Update close button state */
    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->close_pressed && !left) {
            if (!point_in_close_button(win, mx, my)) {
                win->close_pressed = false;
            }
        }
        if (win->minimize_pressed && !left) {
            if (!point_in_minimize_button(win, mx, my)) {
                win->minimize_pressed = false;
            }
        }
        if (win->maximize_pressed && !left) {
            if (!point_in_maximize_button(win, mx, my)) {
                win->maximize_pressed = false;
            }
        }
    }

    /* Update hover resize edge for cursor changes (only if not resizing or dragging) */
    bool any_resizing = false;
    bool any_dragging = false;
    for (int i = 0; i < windows_capacity; i++) {
        if (windows[i].visible && windows[i].resizing) {
            any_resizing = true;
            hover_resize_edge = windows[i].resize_edge;
            break;
        }
        if (windows[i].visible && windows[i].dragging) {
            any_dragging = true;
            break;
        }
    }

    if (!any_resizing && !any_dragging) {
        /* Check from top to bottom for resize edge hover */
        for (int i = num_windows - 1; i >= 0; i--) {
            int slot = window_order[i];
            if (slot >= 0 && windows[slot].visible
                && !(windows[slot].flags & WINDOW_FLAG_MINIMIZED)) {
                if (wm_point_in_window(&windows[slot], mx, my)) {
                    hover_resize_edge = get_resize_edge(&windows[slot], mx, my);
                    break;
                }
            }
        }
    }
}

window_t *wm_check_close_clicked(void)
{
    if (!windows)
        return NULL;

    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool left = input_mouse_left();

    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->close_pressed && !left) {
            if (point_in_close_button(win, mx, my)) {
                win->close_pressed = false;
                return win;
            }
            win->close_pressed = false;
        }
    }
    return NULL;
}

window_t *wm_check_minimize_clicked(void)
{
    if (!windows)
        return NULL;

    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool left = input_mouse_left();

    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->minimize_pressed && !left) {
            if (point_in_minimize_button(win, mx, my)) {
                win->minimize_pressed = false;
                return win;
            }
            win->minimize_pressed = false;
        }
    }
    return NULL;
}

window_t *wm_check_maximize_clicked(void)
{
    if (!windows)
        return NULL;

    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool left = input_mouse_left();

    for (int i = 0; i < windows_capacity; i++) {
        window_t *win = &windows[i];
        if (!win->visible)
            continue;

        if (win->maximize_pressed && !left) {
            if (point_in_maximize_button(win, mx, my)) {
                win->maximize_pressed = false;
                return win;
            }
            win->maximize_pressed = false;
        }
    }
    return NULL;
}

void wm_minimize_window(window_t *win)
{
    if (!win || !win->visible)
        return;
    win->flags |= WINDOW_FLAG_MINIMIZED;
}

void wm_restore_window(window_t *win)
{
    if (!win || !win->visible)
        return;
    win->flags &= ~WINDOW_FLAG_MINIMIZED;
    wm_bring_to_front(win);
}

void wm_toggle_maximize(window_t *win)
{
    if (!win || !win->visible)
        return;

    if (win->flags
        & (WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT | WINDOW_FLAG_SNAPPED_RIGHT)) {
        /* Restore from maximized or snapped state */
        win->bounds = win->saved_bounds;
        win->flags &= ~(
            WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT | WINDOW_FLAG_SNAPPED_RIGHT);
    } else {
        /* Maximize - save current bounds first */
        win->saved_bounds = win->bounds;

        /* Get screen size from graphics */
        gfx_context_t *fb = &g_ctx;

        /* Maximize to full screen minus taskbar (28 pixels at top) */
        win->bounds.x = 0;
        win->bounds.y = 28; /* Below taskbar */
        win->bounds.w = (int) fb->width;
        win->bounds.h = (int) fb->height - 28;

        win->flags |= WINDOW_FLAG_MAXIMIZED;
    }

    /* Update content rect */
    win->content.x = win->bounds.x + 4;
    win->content.y = win->bounds.y + TITLEBAR_HEIGHT + 3;
    win->content.w = win->bounds.w - 8;
    win->content.h = win->bounds.h - TITLEBAR_HEIGHT - 7;
}

static void draw_window_button(int x, int y, bool pressed, bool active)
{
    rect_t btn = {x, y, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE};

    if (pressed) {
        gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(btn), COLOR_DARK_GRAY);
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {x + 1, y + 1, x + WINDOW_BUTTON_SIZE - 2, y + 1, 1},
            COLOR_BEVEL_DARK);
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {x + 1, y + 1, x + 1, y + WINDOW_BUTTON_SIZE - 2, 1},
            COLOR_BEVEL_DARK);
    } else {
        gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(btn), COLOR_PLATINUM);
        if (active) {
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {x + 1, y + 1, x + WINDOW_BUTTON_SIZE - 2, y + 1, 1},
                COLOR_BEVEL_LIGHT);
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {x + 1, y + 1, x + 1, y + WINDOW_BUTTON_SIZE - 2, 1},
                COLOR_BEVEL_LIGHT);
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {x + 1,
                              y + WINDOW_BUTTON_SIZE - 2,
                              x + WINDOW_BUTTON_SIZE - 2,
                              y + WINDOW_BUTTON_SIZE - 2,
                              1},
                COLOR_BEVEL_DARK);
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {x + WINDOW_BUTTON_SIZE - 2,
                              y + 1,
                              x + WINDOW_BUTTON_SIZE - 2,
                              y + WINDOW_BUTTON_SIZE - 2,
                              1},
                COLOR_BEVEL_DARK);
        }
    }
    gfx_draw_rect(&g_ctx, (gfx_rect_t) {btn.x, btn.y, btn.w, btn.h, COLOR_BLACK, 1});
}

static void draw_window(window_t *win)
{
    bool active = (win->flags & WINDOW_FLAG_ACTIVE) != 0;

    /* Draw window shadow (dark theme - deeper shadow) */
    rect_t shadow = {win->bounds.x + 3, win->bounds.y + 3, win->bounds.w, win->bounds.h};
    gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(shadow), COLOR_BLACK);

    /* Draw window frame with dark theme */
    gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(win->bounds), COLOR_PLATINUM);

    /* Draw title bar */
    rect_t titlebar = {win->bounds.x, win->bounds.y, win->bounds.w, TITLEBAR_HEIGHT};

    if (active) {
        /* Active window has gradient title bar - dark theme */
        for (int y = win->bounds.y + 1; y < win->bounds.y + TITLEBAR_HEIGHT - 1; y++) {
            /* Dark gradient from medium gray to darker */
            int progress = y - win->bounds.y;
            int gray_val = 75 - (progress * 25 / TITLEBAR_HEIGHT);
            color_t line_color = COLOR_MAKE(gray_val, gray_val, gray_val + 5, 255);
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {win->bounds.x + 1, y, win->bounds.x + win->bounds.w - 2, y, 1},
                line_color);
        }
        /* Add subtle stripes for active window - start after buttons */
        int num_buttons = 0;
        if (win->flags & WINDOW_FLAG_CLOSABLE)
            num_buttons++;
        if (win->flags & WINDOW_FLAG_MAXIMIZABLE)
            num_buttons++;
        if (win->flags & WINDOW_FLAG_MINIMIZABLE)
            num_buttons++;
        int stripe_start = win->bounds.x + 4
                           + num_buttons * (WINDOW_BUTTON_SIZE + WINDOW_BUTTON_SPACING) + 4;
        int stripe_end = win->bounds.x + win->bounds.w - 10;
        for (int y = win->bounds.y + 2; y < win->bounds.y + TITLEBAR_HEIGHT - 2; y += 2) {
            gfx_draw_line(&g_ctx, (gfx_line_t) {stripe_start, y, stripe_end, y, 1}, COLOR_BEVEL_LIGHT);
        }
    } else {
        /* Inactive window has plain dark title bar */
        gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(titlebar), COLOR_PLATINUM);
    }

    /* Draw outer frame */
    gfx_draw_rect(
        &g_ctx,
        (gfx_rect_t) {win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, COLOR_BLACK, 1});

    /* Draw 3D bevel on outer edge */
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {win->bounds.x + 1,
                      win->bounds.y + 1,
                      win->bounds.x + win->bounds.w - 2,
                      win->bounds.y + 1,
                      1},
        COLOR_BEVEL_LIGHT);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {win->bounds.x + 1,
                      win->bounds.y + 1,
                      win->bounds.x + 1,
                      win->bounds.y + win->bounds.h - 2,
                      1},
        COLOR_BEVEL_LIGHT);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {win->bounds.x + 1,
                      win->bounds.y + win->bounds.h - 2,
                      win->bounds.x + win->bounds.w - 2,
                      win->bounds.y + win->bounds.h - 2,
                      1},
        COLOR_BEVEL_DARK);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {win->bounds.x + win->bounds.w - 2,
                      win->bounds.y + 1,
                      win->bounds.x + win->bounds.w - 2,
                      win->bounds.y + win->bounds.h - 2,
                      1},
        COLOR_BEVEL_DARK);

    /* Draw title bar bottom border */
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {win->bounds.x,
                      win->bounds.y + TITLEBAR_HEIGHT - 1,
                      win->bounds.x + win->bounds.w - 1,
                      win->bounds.y + TITLEBAR_HEIGHT - 1,
                      1},
        COLOR_BLACK);

    /* Draw window control buttons on the left side */
    int btn_y = win->bounds.y + (TITLEBAR_HEIGHT - WINDOW_BUTTON_SIZE) / 2;
    int btn_x = win->bounds.x + 4;

    /* Close button (leftmost) - X icon */
    if (win->flags & WINDOW_FLAG_CLOSABLE) {
        draw_window_button(btn_x, btn_y, win->close_pressed, active);
        /* Draw X icon */
        color_t icon_color = active ? COLOR_TEXT : COLOR_TEXT_DIM;
        /* Draw X with two crossing lines */
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {btn_x + 4,
                          btn_y + 4,
                          btn_x + WINDOW_BUTTON_SIZE - 5,
                          btn_y + WINDOW_BUTTON_SIZE - 5,
                          1},
            icon_color);
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {btn_x + 4,
                          btn_y + WINDOW_BUTTON_SIZE - 5,
                          btn_x + WINDOW_BUTTON_SIZE - 5,
                          btn_y + 4,
                          1},
            icon_color);
        btn_x += WINDOW_BUTTON_SIZE + WINDOW_BUTTON_SPACING;
    }

    /* Maximize button (second) - square icon */
    if (win->flags & WINDOW_FLAG_MAXIMIZABLE) {
        draw_window_button(btn_x, btn_y, win->maximize_pressed, active);
        /* Draw square/restore icon */
        color_t icon_color = active ? COLOR_TEXT : COLOR_TEXT_DIM;
        if (win->flags
            & (WINDOW_FLAG_MAXIMIZED | WINDOW_FLAG_SNAPPED_LEFT | WINDOW_FLAG_SNAPPED_RIGHT)) {
            /* Restore icon - two overlapping squares (shown when maximized or snapped) */
            gfx_draw_rect(&g_ctx, (gfx_rect_t) {btn_x + 5, btn_y + 3, 6, 6, icon_color, 1});
            gfx_draw_rect(&g_ctx, (gfx_rect_t) {btn_x + 3, btn_y + 5, 6, 6, icon_color, 1});
            gfx_draw_filled_rect(
                &g_ctx, TO_GFX_RECT(((rect_t) {btn_x + 4, btn_y + 6, 4, 4})), COLOR_PLATINUM);
        } else {
            /* Maximize icon - single square */
            gfx_draw_rect(
                &g_ctx,
                (gfx_rect_t) {btn_x + 3,
                              btn_y + 3,
                              WINDOW_BUTTON_SIZE - 6,
                              WINDOW_BUTTON_SIZE - 6,
                              icon_color,
                              1});
            gfx_draw_line(
                &g_ctx,
                (gfx_line_t) {btn_x + 3, btn_y + 4, btn_x + WINDOW_BUTTON_SIZE - 4, btn_y + 4, 1},
                icon_color);
        }
        btn_x += WINDOW_BUTTON_SIZE + WINDOW_BUTTON_SPACING;
    }

    /* Minimize button (rightmost of left buttons) - underscore icon */
    if (win->flags & WINDOW_FLAG_MINIMIZABLE) {
        draw_window_button(btn_x, btn_y, win->minimize_pressed, active);
        /* Draw underscore icon */
        color_t icon_color = active ? COLOR_TEXT : COLOR_TEXT_DIM;
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {btn_x + 3,
                          btn_y + WINDOW_BUTTON_SIZE - 5,
                          btn_x + WINDOW_BUTTON_SIZE - 4,
                          btn_y + WINDOW_BUTTON_SIZE - 5,
                          1},
            icon_color);
        gfx_draw_line(
            &g_ctx,
            (gfx_line_t) {btn_x + 3,
                          btn_y + WINDOW_BUTTON_SIZE - 4,
                          btn_x + WINDOW_BUTTON_SIZE - 4,
                          btn_y + WINDOW_BUTTON_SIZE - 4,
                          1},
            icon_color);
    }

    /* Draw title text (centered) */
    int title_w = font_text_width(win->title);
    int title_x = win->bounds.x + (win->bounds.w - title_w) / 2;
    int title_y = win->bounds.y + (TITLEBAR_HEIGHT - font_text_height()) / 2;

    /* Draw background behind title for active window */
    if (active) {
        rect_t title_bg = {title_x - 6, title_y - 2, title_w + 12, font_text_height() + 4};
        gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(title_bg), COLOR_PLATINUM);
    }
    gfx_draw_text(&g_ctx, &g_font, (gfx_pos_t) {title_x, title_y}, COLOR_TEXT, win->title);

    /* Draw content area border (sunken effect - dark theme) */
    rect_t content_border
        = {win->content.x - 1, win->content.y - 1, win->content.w + 2, win->content.h + 2};
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {content_border.x,
                      content_border.y,
                      content_border.x + content_border.w - 1,
                      content_border.y,
                      1},
        COLOR_BEVEL_DARK);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {content_border.x,
                      content_border.y,
                      content_border.x,
                      content_border.y + content_border.h - 1,
                      1},
        COLOR_BEVEL_DARK);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {content_border.x,
                      content_border.y + content_border.h - 1,
                      content_border.x + content_border.w - 1,
                      content_border.y + content_border.h - 1,
                      1},
        COLOR_BEVEL_LIGHT);
    gfx_draw_line(
        &g_ctx,
        (gfx_line_t) {content_border.x + content_border.w - 1,
                      content_border.y,
                      content_border.x + content_border.w - 1,
                      content_border.y + content_border.h - 1,
                      1},
        COLOR_BEVEL_LIGHT);

    /* Draw content area */
    gfx_set_clip(&g_ctx, TO_GFX_RECT(win->content));

    if (win->draw_callback) {
        win->draw_callback(win);
    } else {
        /* Default: white background */
        gfx_draw_filled_rect(&g_ctx, TO_GFX_RECT(win->content), COLOR_WHITE);
    }

    gfx_reset_clip(&g_ctx);
}

void wm_draw_all(void)
{
    if (!windows || !window_order)
        return;

    /* Draw windows from bottom to top */
    for (int i = 0; i < num_windows; i++) {
        int slot = window_order[i];
        if (slot >= 0 && windows[slot].visible && !(windows[slot].flags & WINDOW_FLAG_MINIMIZED)) {
            draw_window(&windows[slot]);
        }
    }
}

void wm_set_draw_callback(window_t *win, void (*callback)(window_t *))
{
    if (win) {
        win->draw_callback = callback;
    }
}

int wm_get_window_count(void)
{
    return num_windows;
}

window_t *wm_get_window_by_creation_order(int index)
{
    if (index < 0 || index >= num_windows || !windows || !window_order)
        return NULL;

    /* Find the nth visible window by walking the window_order array
       but sorting by window ID (which reflects creation order) */
    int count = 0;
    for (int id = 1; id <= next_window_id; id++) {
        for (int i = 0; i < num_windows; i++) {
            int slot = window_order[i];
            if (slot >= 0 && windows[slot].visible && windows[slot].id == id) {
                if (count == index) {
                    return &windows[slot];
                }
                count++;
                break;
            }
        }
    }
    return NULL;
}

resize_edge_t wm_get_hover_resize_edge(void)
{
    return hover_resize_edge;
}

bool wm_is_resizing(void)
{
    if (!windows)
        return false;

    for (int i = 0; i < windows_capacity; i++) {
        if (windows[i].visible && windows[i].resizing) {
            return true;
        }
    }
    return false;
}
