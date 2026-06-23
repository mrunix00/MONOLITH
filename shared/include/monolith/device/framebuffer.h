/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FRAMEBUFFER_DEVICE_COMMAND_GET_INFO = 1,
} framebuffer_device_command_id_t;

typedef struct
{
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
    size_t size;
} framebuffer_device_info_t;
