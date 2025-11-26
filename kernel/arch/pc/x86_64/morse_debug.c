/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/klibc/string.h>
#include <stdbool.h>
#include <stdint.h>

#define MORSE_UNIT 50000000

static void _pause(int units)
{
    for (int i = 0; i < units * MORSE_UNIT; i++)
        ;
}

static void _speaker_on()
{
    uint8_t tmp = asm_inb(0x61);
    if (tmp != (tmp | 3)) {
        asm_outb(tmp | 3, 0x61);
    }
}

static void _speaker_off()
{
    uint8_t tmp = asm_inb(0x61) & 0xFC;
    asm_outb(tmp, 0x61);
}

static void _morse_dot()
{
    _speaker_on();
    _pause(1);
    _speaker_off();
}

static void _morse_dash()
{
    _speaker_on();
    _pause(3);
    _speaker_off();
}

static bool _is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool _is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static char _to_lowercase(char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static void _morse_char(const char *s)
{
    while (*s != '\0') {
        if (*s == '.') {
            _morse_dot();
        } else if (*s == '_') {
            _morse_dash();
        }
        _pause(1);
        s++;
    }
}

void morse_log(const char *message)
{
    static const char *digit_map[]
        = {"_____", ".____", "..___", "...__", "...._", ".....", "_....", "__...", "___..", "____."};
    static const char *char_map[] = {"._",   "_...", "_._.",  "_..",  ".",    ".._.", "__.",
                                     "....", "..",   ".____", "_._",  "._..", "__",   "_.",
                                     "___",  ".__.", "__._",  "._.",  "...",  "_",    ".._",
                                     "..._", ".__",  "_.._",  "_.__", "__.."};
    while (*message != '\0') {
        if (_is_digit(*message)) {
            _morse_char(digit_map[*message - '0']);
            _pause(2);
        } else if (_is_alpha(*message)) {
            _morse_char(char_map[_to_lowercase(*message) - 'a']);
            _pause(2);
        } else if (*message == ' ') {
            _pause(7);
        }
        message++;
    }
}
