/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

/* KAYPRO10 8x16 bitmap font (80 column variation, 8 hdots) */
#define FONT_WIDTH  8
#define FONT_HEIGHT 18
#define FONT_FIRST_CHAR 0
#define FONT_LAST_CHAR 255

/* Get raw font data */
const uint8_t *font_get_data(void);

/* Get font bitmap for a character (returns pointer to 12 bytes, each byte is a row) */
const uint8_t *font_get_glyph(char c);

/* Get width of a string in pixels */
int font_text_width(const char *text);

/* Get height of text in pixels */
int font_text_height(void);
