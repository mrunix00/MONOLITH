/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 *
 * MONOLITH Desktop - Retro style desktop environment
 */

#include "desktop.h"
#include "font.h"
#include "graphics.h"
#include "input.h"
#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Application state */
static bool _running = true;

/* Demo window content */
static char _about_text[] = "MONOLITH Desktop\n\nA graphical desktop environment \nfor MONOLITH.";

/* ============================================================================
 * New Folder Dialog System
 * ============================================================================ */

#define MAX_FOLDER_NAME_LEN 31
#define DIALOG_INPUT_HEIGHT 22
#define DIALOG_BUTTON_WIDTH 70
#define DIALOG_BUTTON_HEIGHT 24

/* Dialog state */
typedef struct
{
    char input_text[MAX_FOLDER_NAME_LEN + 1];
    int cursor_pos;
    bool is_active;
    window_t *dialog_window;
} new_folder_dialog_t;

static new_folder_dialog_t _folder_dialog = {0};

/* Create the folder and close dialog */
static void _create_folder_from_dialog(void)
{
    if (!_folder_dialog.is_active)
        return;

    /* Use default name if empty */
    const char *name = _folder_dialog.input_text;
    if (strlen(name) == 0) {
        name = "New Folder";
    }

    /* Create the folder icon on desktop */
    desktop_add_icon_grid(name);

    /* Close dialog */
    _folder_dialog.is_active = false;
    if (_folder_dialog.dialog_window) {
        wm_destroy_window(_folder_dialog.dialog_window);
        _folder_dialog.dialog_window = NULL;
    }
}

/* Process input for the new folder dialog */
static void _process_folder_dialog_input(void)
{
    if (!_folder_dialog.is_active || !_folder_dialog.dialog_window)
        return;

    /* Only process input if this dialog is the active window */
    if (wm_get_active() != _folder_dialog.dialog_window)
        return;

    char c = input_get_char();

    /* No input this frame */
    if (c == 0) {
        /* Check arrow keys even without character input */
        if (input_key_pressed(KEY_LEFT) && _folder_dialog.cursor_pos > 0) {
            _folder_dialog.cursor_pos--;
        }
        if (input_key_pressed(KEY_RIGHT)
            && _folder_dialog.cursor_pos < (int) strlen(_folder_dialog.input_text)) {
            _folder_dialog.cursor_pos++;
        }
        return;
    }

    if (c == '\n') {
        /* Enter pressed - create folder */
        _create_folder_from_dialog();
        return;
    }

    if (c == '\b') {
        /* Backspace - delete character before cursor */
        if (_folder_dialog.cursor_pos > 0) {
            /* Shift characters left */
            int len = strlen(_folder_dialog.input_text);
            for (int i = _folder_dialog.cursor_pos - 1; i < len; i++) {
                _folder_dialog.input_text[i] = _folder_dialog.input_text[i + 1];
            }
            _folder_dialog.cursor_pos--;
        }
        return;
    }

    if (c == 27) { /* Escape */
        /* Cancel dialog */
        _folder_dialog.is_active = false;
        wm_destroy_window(_folder_dialog.dialog_window);
        _folder_dialog.dialog_window = NULL;
        return;
    }

    /* Regular character input */
    if (c >= 32 && c < 127) {
        int len = strlen(_folder_dialog.input_text);
        if (len < MAX_FOLDER_NAME_LEN) {
            /* Insert character at cursor position */
            for (int i = len + 1; i > _folder_dialog.cursor_pos; i--) {
                _folder_dialog.input_text[i] = _folder_dialog.input_text[i - 1];
            }
            _folder_dialog.input_text[_folder_dialog.cursor_pos] = c;
            _folder_dialog.cursor_pos++;
        }
    }
}

/* Draw the new folder dialog content */
static void _draw_new_folder_dialog(window_t *win)
{
    /* Dark background */
    graphics_fill_rect(win->content, COLOR_DARK_GRAY);

    int pad = 12;
    int x = win->content.x + pad;
    int y = win->content.y + pad;

    /* Draw label */
    graphics_draw_text("Enter folder name:", x, y, COLOR_TEXT);
    y += FONT_HEIGHT + 8;

    /* Draw text input field */
    rect_t input_rect = {x, y, win->content.w - pad * 2, DIALOG_INPUT_HEIGHT};

    /* Input field background (white/light for text entry) */
    graphics_fill_rect(input_rect, COLOR_MAKE(240, 240, 240, 255));

    /* 3D sunken border */
    graphics_hline(input_rect.x, input_rect.x + input_rect.w - 1, input_rect.y, COLOR_BEVEL_DARK);
    graphics_vline(input_rect.x, input_rect.y, input_rect.y + input_rect.h - 1, COLOR_BEVEL_DARK);
    graphics_hline(
        input_rect.x,
        input_rect.x + input_rect.w - 1,
        input_rect.y + input_rect.h - 1,
        COLOR_BEVEL_LIGHT);
    graphics_vline(
        input_rect.x + input_rect.w - 1,
        input_rect.y,
        input_rect.y + input_rect.h - 1,
        COLOR_BEVEL_LIGHT);
    graphics_draw_rect(input_rect, COLOR_BLACK);

    /* Draw input text */
    int text_x = input_rect.x + 4;
    int text_y = input_rect.y + (DIALOG_INPUT_HEIGHT - FONT_HEIGHT) / 2;
    graphics_draw_text(_folder_dialog.input_text, text_x, text_y, COLOR_BLACK);

    /* Draw cursor if active */
    if (_folder_dialog.is_active && wm_get_active() == win) {
        /* Calculate cursor position */
        char temp[MAX_FOLDER_NAME_LEN + 1];
        strncpy(temp, _folder_dialog.input_text, _folder_dialog.cursor_pos);
        temp[_folder_dialog.cursor_pos] = '\0';
        int cursor_x = text_x + font_text_width(temp);

        /* Draw cursor line */
        graphics_vline(cursor_x, text_y, text_y + FONT_HEIGHT - 1, COLOR_BLACK);
    }

    y += DIALOG_INPUT_HEIGHT + 16;

    /* Draw buttons */
    int btn_y = win->content.y + win->content.h - DIALOG_BUTTON_HEIGHT - pad;
    int btn_spacing = 10;

    /* OK button */
    rect_t ok_btn
        = {win->content.x + win->content.w - pad - DIALOG_BUTTON_WIDTH * 2 - btn_spacing,
           btn_y,
           DIALOG_BUTTON_WIDTH,
           DIALOG_BUTTON_HEIGHT};

    /* Cancel button */
    rect_t cancel_btn
        = {win->content.x + win->content.w - pad - DIALOG_BUTTON_WIDTH,
           btn_y,
           DIALOG_BUTTON_WIDTH,
           DIALOG_BUTTON_HEIGHT};

    /* Check for button hover/click */
    int mx = input_get_mouse_x();
    int my = input_get_mouse_y();
    bool ok_hover
        = (mx >= ok_btn.x && mx < ok_btn.x + ok_btn.w && my >= ok_btn.y && my < ok_btn.y + ok_btn.h);
    bool cancel_hover
        = (mx >= cancel_btn.x && mx < cancel_btn.x + cancel_btn.w && my >= cancel_btn.y
           && my < cancel_btn.y + cancel_btn.h);

    /* Draw OK button */
    if (ok_hover) {
        graphics_fill_rect(ok_btn, COLOR_HIGHLIGHT);
    } else {
        graphics_fill_rect(ok_btn, COLOR_PLATINUM);
    }
    /* 3D border */
    graphics_hline(
        ok_btn.x + 1,
        ok_btn.x + ok_btn.w - 2,
        ok_btn.y + 1,
        ok_hover ? COLOR_HIGHLIGHT : COLOR_BEVEL_LIGHT);
    graphics_vline(
        ok_btn.x + 1,
        ok_btn.y + 1,
        ok_btn.y + ok_btn.h - 2,
        ok_hover ? COLOR_HIGHLIGHT : COLOR_BEVEL_LIGHT);
    graphics_hline(ok_btn.x + 1, ok_btn.x + ok_btn.w - 2, ok_btn.y + ok_btn.h - 2, COLOR_BEVEL_DARK);
    graphics_vline(ok_btn.x + ok_btn.w - 2, ok_btn.y + 1, ok_btn.y + ok_btn.h - 2, COLOR_BEVEL_DARK);
    graphics_draw_rect(ok_btn, COLOR_BLACK);

    /* OK text */
    const char *ok_text = "OK";
    int ok_text_x = ok_btn.x + (ok_btn.w - font_text_width(ok_text)) / 2;
    int ok_text_y = ok_btn.y + (ok_btn.h - FONT_HEIGHT) / 2;
    graphics_draw_text(ok_text, ok_text_x, ok_text_y, ok_hover ? COLOR_WHITE : COLOR_TEXT);

    /* Draw Cancel button */
    if (cancel_hover) {
        graphics_fill_rect(cancel_btn, COLOR_HIGHLIGHT);
    } else {
        graphics_fill_rect(cancel_btn, COLOR_PLATINUM);
    }
    /* 3D border */
    graphics_hline(
        cancel_btn.x + 1,
        cancel_btn.x + cancel_btn.w - 2,
        cancel_btn.y + 1,
        cancel_hover ? COLOR_HIGHLIGHT : COLOR_BEVEL_LIGHT);
    graphics_vline(
        cancel_btn.x + 1,
        cancel_btn.y + 1,
        cancel_btn.y + cancel_btn.h - 2,
        cancel_hover ? COLOR_HIGHLIGHT : COLOR_BEVEL_LIGHT);
    graphics_hline(
        cancel_btn.x + 1,
        cancel_btn.x + cancel_btn.w - 2,
        cancel_btn.y + cancel_btn.h - 2,
        COLOR_BEVEL_DARK);
    graphics_vline(
        cancel_btn.x + cancel_btn.w - 2,
        cancel_btn.y + 1,
        cancel_btn.y + cancel_btn.h - 2,
        COLOR_BEVEL_DARK);
    graphics_draw_rect(cancel_btn, COLOR_BLACK);

    /* Cancel text */
    const char *cancel_text = "Cancel";
    int cancel_text_x = cancel_btn.x + (cancel_btn.w - font_text_width(cancel_text)) / 2;
    int cancel_text_y = cancel_btn.y + (cancel_btn.h - FONT_HEIGHT) / 2;
    graphics_draw_text(
        cancel_text, cancel_text_x, cancel_text_y, cancel_hover ? COLOR_WHITE : COLOR_TEXT);

    /* Handle button clicks */
    if (input_mouse_left_clicked()) {
        if (ok_hover) {
            _create_folder_from_dialog();
        } else if (cancel_hover) {
            _folder_dialog.is_active = false;
            wm_destroy_window(_folder_dialog.dialog_window);
            _folder_dialog.dialog_window = NULL;
        }
    }
}

/* Menu callbacks */
/* Window content draw callbacks */
static void _draw_about_content(window_t *win)
{
    /* Dark theme window content */
    graphics_fill_rect(win->content, COLOR_DARK_GRAY);

    char *text = (char *) win->user_data;
    if (!text)
        return;

    int x = win->content.x + 10;
    int y = win->content.y + 10;
    int line_start = 0;
    int i = 0;

    while (text[i]) {
        if (text[i] == '\n') {
            /* Draw this line */
            char line[64];
            int len = i - line_start;
            if (len > 63)
                len = 63;
            strncpy(line, text + line_start, len);
            line[len] = '\0';

            graphics_draw_text(line, x, y, COLOR_TEXT);
            y += FONT_HEIGHT + 2;
            line_start = i + 1;
        }
        i++;
    }

    /* Draw last line */
    if (i > line_start) {
        graphics_draw_text(text + line_start, x, y, COLOR_TEXT);
    }
}

static void _menu_about(void)
{
    /* Create About window */
    window_t *win = wm_create_window(
        "About MONOLITH",
        150,
        80,
        250,
        180,
        /* TODO: Get rid of WINDOW_FLAG_CLOSABLE and WINDOW_FLAG_DRAGGABLE */
        /* TODO: Add support for disabling window decorations */
        WINDOW_FLAG_CLOSABLE | WINDOW_FLAG_DRAGGABLE | WINDOW_FLAG_RESIZABLE
            | WINDOW_FLAG_MINIMIZABLE | WINDOW_FLAG_MAXIMIZABLE);
    if (win) {
        win->user_data = _about_text;
        wm_set_draw_callback(win, _draw_about_content);
    }
}

static void _menu_quit(void)
{
    _running = false;
}

/* Context menu callbacks */
static void _ctx_view_refresh(void)
{
    /* Refresh desktop view - placeholder */
}

static void _ctx_new_folder(void)
{
    /* Initialize dialog state */
    memset(_folder_dialog.input_text, 0, sizeof(_folder_dialog.input_text));
    strcpy(_folder_dialog.input_text, "New Folder");
    _folder_dialog.cursor_pos = strlen(_folder_dialog.input_text);
    _folder_dialog.is_active = true;

    /* Create dialog window */
    framebuffer_t *fb = graphics_get_fb();
    int dialog_w = 300;
    int dialog_h = 130;
    int dialog_x = ((int) fb->width - dialog_w) / 2;
    int dialog_y = ((int) fb->height - dialog_h) / 2;

    window_t *win = wm_create_window(
        "New Folder",
        dialog_x,
        dialog_y,
        dialog_w,
        dialog_h,
        WINDOW_FLAG_CLOSABLE | WINDOW_FLAG_DRAGGABLE);
    if (win) {
        _folder_dialog.dialog_window = win;
        wm_set_draw_callback(win, _draw_new_folder_dialog);
        wm_bring_to_front(win);
    } else {
        _folder_dialog.is_active = false;
    }
}

static void _ctx_delete_icon(void)
{
    int selected = desktop_get_selected_icon();
    if (selected >= 0) {
        desktop_delete_icon(selected);
    }
}

/* Context menu builder - called when right-clicking on desktop */
static void _build_desktop_context_menu()
{
    /* Check if an icon is selected (right-clicked on an icon) */
    int selected_icon = desktop_get_selected_icon();

    if (selected_icon >= 0) {
        /* Icon context menu */
        desktop_add_context_menu_item("Open", NULL, NULL); /* TODO: implement open */
        desktop_add_context_menu_separator();
        desktop_add_context_menu_item("Delete", "Del", _ctx_delete_icon);
    } else {
        /* Empty desktop context menu */
        desktop_add_context_menu_item("Refresh", "F5", _ctx_view_refresh);
        desktop_add_context_menu_item("New Folder", NULL, _ctx_new_folder);
    }
}

static void _launch_demo()
{
    syscall1(SYSCALL_SPAWN_TASK, (long) "system:/sched_demo");
}

/* Setup the desktop environment */
static void _setup_desktop(void)
{
    /* Initialize desktop and window manager */
    desktop_init();
    wm_init();

    /* Set up context menu builder for right-click menus */
    desktop_set_context_menu_builder(_build_desktop_context_menu);

    /* Set up the main menu items */
    desktop_add_menu_item("About MONOLITH...", _menu_about);
    desktop_add_menu_item("Launch Demo", _launch_demo);
    desktop_add_menu_item("Exit", _menu_quit);

    /* Add desktop icons - arranged in grid */
    desktop_add_icon_grid("System Disk");
    desktop_add_icon_grid("Trash");

    /* Create initial "About" window */
    window_t *about_win = wm_create_window(
        "Welcome",
        80,
        60,
        280,
        200,
        WINDOW_FLAG_CLOSABLE | WINDOW_FLAG_DRAGGABLE | WINDOW_FLAG_RESIZABLE
            | WINDOW_FLAG_MINIMIZABLE | WINDOW_FLAG_MAXIMIZABLE);
    if (about_win) {
        about_win->user_data = _about_text;
        wm_set_draw_callback(about_win, _draw_about_content);
    }
}

int main(void)
{
    /* Initialize graphics */
    if (graphics_init() < 0)
        exit();

    input_init();
    _setup_desktop();

    /* Initial render */
    desktop_draw();
    desktop_draw_cursor();
    graphics_present();

    /* Main loop - event driven rendering */
    while (_running) {
        /* Wait for input events (sleep to avoid busy-waiting) */
        while (!input_has_events() && _running)
            usleep(1000 / 60);

        /* Update input state at start of frame */
        input_update();

        /* Process input for dialogs first */
        _process_folder_dialog_input();

        /* Process input */
        bool click_consumed = desktop_process_input();
        wm_process_input(click_consumed);

        /* Update cursor based on resize edge hover */
        switch ((int) wm_get_hover_resize_edge()) {
        case RESIZE_NONE:
            desktop_set_cursor(CURSOR_ARROW);
            break;
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            desktop_set_cursor(CURSOR_RESIZE_NS);
            break;
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            desktop_set_cursor(CURSOR_RESIZE_EW);
            break;
        case RESIZE_TOP | RESIZE_LEFT:
        case RESIZE_BOTTOM | RESIZE_RIGHT:
            desktop_set_cursor(CURSOR_RESIZE_NWSE);
            break;
        case RESIZE_TOP | RESIZE_RIGHT:
        case RESIZE_BOTTOM | RESIZE_LEFT:
            desktop_set_cursor(CURSOR_RESIZE_NESW);
            break;
        }

        /* Check for window close clicks */
        window_t *closed = wm_check_close_clicked();
        if (closed) {
            /* Check if this is the folder dialog being closed */
            if (closed == _folder_dialog.dialog_window) {
                _folder_dialog.is_active = false;
                _folder_dialog.dialog_window = NULL;
            }
            wm_destroy_window(closed);
        }

        /* Check for window minimize clicks */
        window_t *minimized = wm_check_minimize_clicked();
        if (minimized) {
            wm_minimize_window(minimized);
        }

        /* Check for window maximize clicks */
        window_t *maximized = wm_check_maximize_clicked();
        if (maximized) {
            wm_toggle_maximize(maximized);
        }

        /* Draw everything */
        desktop_draw();

        /* Draw cursor on top */
        desktop_draw_cursor();

        /* Present to screen */
        graphics_present();

        /* Clear events flag after processing */
        input_clear_events();
    }

    /* Exit cleanly */
    exit();
    return 0;
}
