/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_FRAMEBUFFERS 8

typedef struct
{
    uint32_t *address;
    size_t width;
    size_t height;
    size_t pitch;
    size_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} framebuffer_t;

void setup_framebuffer(framebuffer_t fb);
framebuffer_t *get_framebuffer(uint8_t index);
