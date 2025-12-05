/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "input.h"
#include "graphics.h"
#include <sys/syscall.h>
#include <unistd.h>

/* Mouse state */
static int mouse_x = 100;
static int mouse_y = 100;
static bool mouse_left = false;
static bool mouse_right = false;
static bool mouse_middle = false;
static bool prev_mouse_left = false;
static bool prev_mouse_right = false;

/* Click events (set by interrupt handler, cleared by input_update) */
static volatile bool left_click_event = false;
static volatile bool right_click_event = false;

/* Frame-level click state (set at start of frame, valid for entire frame) */
static bool frame_left_clicked = false;
static bool frame_right_clicked = false;

/* Keyboard state */
static bool shift_held = false;
static bool capslock_on = false;
static volatile char last_char = 0;
static volatile bool char_available = false; /* Flag to indicate new char available */
static keyboard_scancode_t last_key_pressed = 0;

/* Scancode to ASCII mapping */
static const char scancode_to_ascii[256] = {0,    0,   '1', '2',  '3',  '4',  '5', '6', '7',  '8',
                                            '9',  '0', '-', '=',  '\b', '\t', 'q', 'w', 'e',  'r',
                                            't',  'y', 'u', 'i',  'o',  'p',  '[', ']', '\n', 0,
                                            'a',  's', 'd', 'f',  'g',  'h',  'j', 'k', 'l',  ';',
                                            '\'', '`', 0,   '\\', 'z',  'x',  'c', 'v', 'b',  'n',
                                            'm',  ',', '.', '/',  0,    '*',  0,   ' ', 0,    0,
                                            0,    0,   0,   0,    0,    0,    0,   0,   0,    0,
                                            0,    '7', '8', '9',  '-',  '4',  '5', '6', '+',  '1',
                                            '2',  '3', '0', '.',  0,    0,    0,   0};

static const char scancode_to_ascii_shifted[256]
    = {0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(',  ')', '_', '+', '\b', '\t', 'Q', 'W',
       'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S', 'D',  'F',  'G', 'H',
       'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C',  'V', 'B', 'N', 'M',  '<',  '>', '?',
       0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,    0,    0,   0,
       0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,    0,   0,   0,   0,    0};

static void mouse_handler(mouse_event_t event)
{
    framebuffer_t *fb = graphics_get_fb();

    /* Calculate delta with sign extension */
    int delta_x = event.x_sign ? ((int) event.x_movement - 256) : event.x_movement;
    int delta_y = event.y_sign ? ((int) event.y_movement - 256) : event.y_movement;

    /* Update position */
    int new_x = mouse_x + delta_x;
    int new_y = mouse_y - delta_y; /* Y is inverted */

    /* Clamp to screen bounds */
    if (new_x < 0)
        new_x = 0;
    if (new_y < 0)
        new_y = 0;
    if (new_x >= (int) fb->width)
        new_x = (int) fb->width - 1;
    if (new_y >= (int) fb->height)
        new_y = (int) fb->height - 1;

    mouse_x = new_x;
    mouse_y = new_y;

    /* Detect click events (button just pressed) */
    if (event.left_button && !mouse_left) {
        left_click_event = true;
    }
    if (event.right_button && !mouse_right) {
        right_click_event = true;
    }

    /* Update button state */
    mouse_left = event.left_button;
    mouse_right = event.right_button;
    mouse_middle = event.middle_button;
}

static void keyboard_handler(keyboard_event_t event)
{
    /* Handle modifier keys */
    if (event.scancode == KEY_LSHIFT || event.scancode == KEY_RSHIFT) {
        shift_held = (event.action == KEYBOARD_PRESSED || event.action == KEYBOARD_HOLD);
        return;
    }

    if (event.scancode == KEY_CAPSLOCK && event.action == KEYBOARD_PRESSED) {
        capslock_on = !capslock_on;
        return;
    }

    /* Process key presses and holds (for key repeat) */
    /* KEYBOARD_PRESSED = key just went down, KEYBOARD_HOLD = key is being held */
    if (event.action == KEYBOARD_PRESSED || event.action == KEYBOARD_HOLD) {
        last_key_pressed = event.scancode;

        /* Convert to ASCII */
        uint8_t scancode = (uint8_t) event.scancode;
        bool use_shifted = shift_held;

        /* For letters, also consider capslock */
        char base = scancode_to_ascii[scancode];
        if (base >= 'a' && base <= 'z') {
            use_shifted = shift_held ^ capslock_on;
        }

        if (use_shifted) {
            last_char = scancode_to_ascii_shifted[scancode];
        } else {
            last_char = scancode_to_ascii[scancode];
        }
        char_available = true;
    }
}

void input_init(void)
{
    syscall1(SYSCALL_REGISTER_MOUSE, (long) mouse_handler);
    syscall1(SYSCALL_REGISTER_KEYBOARD, (long) keyboard_handler);
}

int input_get_mouse_x(void)
{
    return mouse_x;
}

int input_get_mouse_y(void)
{
    return mouse_y;
}

bool input_mouse_left(void)
{
    return mouse_left;
}

bool input_mouse_right(void)
{
    return mouse_right;
}

bool input_mouse_middle(void)
{
    return mouse_middle;
}

bool input_mouse_left_clicked(void)
{
    return frame_left_clicked;
}

bool input_mouse_right_clicked(void)
{
    return frame_right_clicked;
}

char input_get_char(void)
{
    if (!char_available) {
        return 0;
    }
    char c = last_char;
    last_char = 0;
    char_available = false;
    return c;
}

bool input_key_pressed(keyboard_scancode_t key)
{
    if (last_key_pressed == key) {
        last_key_pressed = 0;
        return true;
    }
    return false;
}

void input_update(void)
{
    /* Capture click events for this frame */
    frame_left_clicked = left_click_event;
    frame_right_clicked = right_click_event;

    /* Clear the interrupt-set events */
    left_click_event = false;
    right_click_event = false;

    prev_mouse_left = mouse_left;
    prev_mouse_right = mouse_right;
    last_key_pressed = 0;
}
