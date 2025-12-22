/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include "input.h"
#include "graphics.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Mouse state structure matching kernel */
typedef struct
{
    int32_t x;
    int32_t y;
    bool left_button;
    bool right_button;
    bool middle_button;
    int8_t delta_x;
    int8_t delta_y;
} mouse_state_t;

/* Mouse state */
static int mouse_x = 100;
static int mouse_y = 100;
static int prev_mouse_x = 100;
static int prev_mouse_y = 100;
static bool mouse_left = false;
static bool mouse_right = false;
static bool mouse_middle = false;
static bool prev_mouse_left = false;
static bool prev_mouse_right = false;

/* Click events (detected by polling, cleared by input_update) */
static bool left_click_event = false;
static bool right_click_event = false;

/* Frame-level click state (set at start of frame, valid for entire frame) */
static bool frame_left_clicked = false;
static bool frame_right_clicked = false;

/* Event tracking - set when any input event occurs */
static bool events_pending = false;

/* Keyboard state */
static keyboard_action_t prev_key_state[256];
static keyboard_action_t current_key_state[256];
static bool shift_held = false;
static bool capslock_on = false;
static char last_char = 0;
static bool char_available = false; /* Flag to indicate new char available */
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

void input_init(void)
{
    /* Initialize keyboard state */
    memset(prev_key_state, KEYBOARD_RELEASED, sizeof(prev_key_state));
    memset(current_key_state, KEYBOARD_RELEASED, sizeof(current_key_state));

    /* Poll initial state */
    syscall1(SYSCALL_GET_KEYBOARD_STATE, (long) current_key_state);
    memcpy(prev_key_state, current_key_state, sizeof(current_key_state));
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
    framebuffer_t *fb = graphics_get_fb();
    mouse_state_t mouse_state;
    bool mouse_changed = false;
    bool keyboard_changed = false;

    /* Poll mouse state */
    if (syscall1(SYSCALL_GET_MOUSE_STATE, (long) &mouse_state) == 0) {
        /* Update mouse position with clamping */
        int new_x = mouse_state.x;
        int new_y = mouse_state.y;

        /* Clamp to screen bounds */
        if (new_x < 0)
            new_x = 0;
        if (new_y < 0)
            new_y = 0;
        if (new_x >= (int) fb->width)
            new_x = (int) fb->width - 1;
        if (new_y >= (int) fb->height)
            new_y = (int) fb->height - 1;

        /* Detect position or button changes */
        if (new_x != mouse_x || new_y != mouse_y || mouse_state.left_button != mouse_left
            || mouse_state.right_button != mouse_right
            || mouse_state.middle_button != mouse_middle) {
            mouse_changed = true;
        }

        /* Detect click events (button just pressed) */
        if (mouse_state.left_button && !mouse_left) {
            left_click_event = true;
        }
        if (mouse_state.right_button && !mouse_right) {
            right_click_event = true;
        }

        mouse_x = new_x;
        mouse_y = new_y;
        mouse_left = mouse_state.left_button;
        mouse_right = mouse_state.right_button;
        mouse_middle = mouse_state.middle_button;
    }

    /* Poll keyboard state */
    syscall1(SYSCALL_GET_KEYBOARD_STATE, (long) current_key_state);

    /* Check for changes */
    for (int i = 0; i < 256; i++) {
        if (current_key_state[i] != prev_key_state[i]) {
            keyboard_changed = true;
            break;
        }
    }

    /* Process keyboard state changes */
    if (keyboard_changed) {
        /* Handle modifier keys */
        bool new_shift_held
            = (current_key_state[KEY_LSHIFT] == KEYBOARD_PRESSED
               || current_key_state[KEY_LSHIFT] == KEYBOARD_HOLD
               || current_key_state[KEY_RSHIFT] == KEYBOARD_PRESSED
               || current_key_state[KEY_RSHIFT] == KEYBOARD_HOLD);

        if (new_shift_held != shift_held) {
            shift_held = new_shift_held;
        }

        /* Handle capslock toggle */
        if (current_key_state[KEY_CAPSLOCK] == KEYBOARD_PRESSED
            && prev_key_state[KEY_CAPSLOCK] != KEYBOARD_PRESSED) {
            capslock_on = !capslock_on;
        }

        /* Process key presses and holds (for key repeat) */
        for (int i = 0; i < 256; i++) {
            keyboard_action_t prev_action = prev_key_state[i];
            keyboard_action_t curr_action = current_key_state[i];

            if ((curr_action == KEYBOARD_PRESSED || curr_action == KEYBOARD_HOLD)
                && (prev_action != KEYBOARD_PRESSED && prev_action != KEYBOARD_HOLD)) {
                /* Key was just pressed */
                last_key_pressed = (keyboard_scancode_t) i;

                /* Convert to ASCII */
                uint8_t scancode = (uint8_t) i;
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

        /* Update previous state */
        memcpy(prev_key_state, current_key_state, sizeof(current_key_state));
    }

    /* Capture click events for this frame */
    frame_left_clicked = left_click_event;
    frame_right_clicked = right_click_event;

    /* Clear the click events */
    left_click_event = false;
    right_click_event = false;

    /* Update events_pending flag */
    events_pending = mouse_changed || keyboard_changed;

    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    prev_mouse_left = mouse_left;
    prev_mouse_right = mouse_right;
    last_key_pressed = 0;
}

bool input_has_events(void)
{
    /* Poll kernel state to check for changes without modifying local state */
    mouse_state_t mouse_state;
    keyboard_action_t key_state[256];

    /* Check mouse state */
    if (syscall1(SYSCALL_GET_MOUSE_STATE, (long) &mouse_state) == 0) {
        framebuffer_t *fb = graphics_get_fb();
        int new_x = mouse_state.x;
        int new_y = mouse_state.y;

        /* Clamp to screen bounds */
        if (new_x < 0)
            new_x = 0;
        if (new_y < 0)
            new_y = 0;
        if (new_x >= (int) fb->width)
            new_x = (int) fb->width - 1;
        if (new_y >= (int) fb->height)
            new_y = (int) fb->height - 1;

        /* Check if mouse state changed */
        if (new_x != mouse_x || new_y != mouse_y || mouse_state.left_button != mouse_left
            || mouse_state.right_button != mouse_right
            || mouse_state.middle_button != mouse_middle) {
            return true;
        }
    }

    /* Check keyboard state */
    if (syscall1(SYSCALL_GET_KEYBOARD_STATE, (long) key_state) == 0) {
        /* Compare with previous state */
        for (int i = 0; i < 256; i++) {
            if (key_state[i] != prev_key_state[i]) {
                return true;
            }
        }
    }

    return false;
}

void input_clear_events(void)
{
    events_pending = false;
}
