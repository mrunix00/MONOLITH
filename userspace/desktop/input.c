/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "input.h"
#include <libgfx.h>
#include "types.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Mouse state */
static int mouse_x = 100;
static int mouse_y = 100;
static bool mouse_left = false;
static bool mouse_right = false;
static bool mouse_middle = false;

/* Click events (detected by polling, cleared by input_update) */
static bool left_click_event = false;
static bool right_click_event = false;

/* Frame-level click state (set at start of frame, valid for entire frame) */
static bool frame_left_clicked = false;
static bool frame_right_clicked = false;

/* Event tracking - set when any input event occurs */
static bool events_pending = false;

/* Keyboard state */
static keyboard_action_t current_key_state[256];
static bool shift_held = false;
static bool capslock_on = false;
static char last_char = 0;
static bool char_available = false; /* Flag to indicate new char available */
static keyboard_scancode_t last_key_pressed = 0;
static input_event_t pending_event;
static bool pending_event_valid = false;

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

void input_init(void)
{
    /* Initialize keyboard state */
    memset(current_key_state, KEYBOARD_RELEASED, sizeof(current_key_state));
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
    gfx_context_t *fb = &g_ctx;
    bool mouse_changed = false;
    bool keyboard_changed = false;
    bool processed_event = false;

    input_event_t event;
    if (pending_event_valid) {
        event = pending_event;
        pending_event_valid = false;
        processed_event = true;
    }

    while (processed_event || syscall1(SYSCALL_POLL_INPUT_EVENT, (long) &event) == 0) {
        processed_event = false;
        if (event.type == INPUT_EVENT_MOUSE) {
            int new_x = event.data.mouse.x;
            int new_y = event.data.mouse.y;

            if (new_x < 0)
                new_x = 0;
            if (new_y < 0)
                new_y = 0;
            if (new_x >= (int) fb->width)
                new_x = (int) fb->width - 1;
            if (new_y >= (int) fb->height)
                new_y = (int) fb->height - 1;

            bool new_left = (event.data.mouse.buttons & (1 << 0)) != 0;
            bool new_right = (event.data.mouse.buttons & (1 << 1)) != 0;
            bool new_middle = (event.data.mouse.buttons & (1 << 2)) != 0;

            if (new_x != mouse_x || new_y != mouse_y || new_left != mouse_left || new_right != mouse_right
                || new_middle != mouse_middle) {
                mouse_changed = true;
            }

            if (new_left && !mouse_left) {
                left_click_event = true;
            }
            if (new_right && !mouse_right) {
                right_click_event = true;
            }

            mouse_x = new_x;
            mouse_y = new_y;
            mouse_left = new_left;
            mouse_right = new_right;
            mouse_middle = new_middle;
        } else if (event.type == INPUT_EVENT_KEYBOARD) {
            uint8_t scancode = event.data.keyboard.scancode;
            keyboard_action_t action = (keyboard_action_t) event.data.keyboard.action;

            if (scancode < 256) {
                current_key_state[scancode] = action;
                keyboard_changed = true;

                if (action == KEYBOARD_PRESSED && scancode == KEY_CAPSLOCK) {
                    capslock_on = !capslock_on;
                }

                if (action == KEYBOARD_PRESSED || action == KEYBOARD_HOLD) {
                    last_key_pressed = (keyboard_scancode_t) scancode;

                    bool use_shifted = shift_held;
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

            shift_held
                = (current_key_state[KEY_LSHIFT] == KEYBOARD_PRESSED
                   || current_key_state[KEY_LSHIFT] == KEYBOARD_HOLD
                   || current_key_state[KEY_RSHIFT] == KEYBOARD_PRESSED
                   || current_key_state[KEY_RSHIFT] == KEYBOARD_HOLD);
        }
    }

    /* Capture click events for this frame */
    frame_left_clicked = left_click_event;
    frame_right_clicked = right_click_event;

    /* Clear the click events */
    left_click_event = false;
    right_click_event = false;

    /* Update events_pending flag */
    events_pending = mouse_changed || keyboard_changed;
    last_key_pressed = 0;
}

bool input_has_events(void)
{
    if (pending_event_valid)
        return true;

    if (syscall1(SYSCALL_POLL_INPUT_EVENT, (long) &pending_event) == 0) {
        pending_event_valid = true;
        return true;
    }

    return events_pending;
}

void input_clear_events(void)
{
    events_pending = false;
}
