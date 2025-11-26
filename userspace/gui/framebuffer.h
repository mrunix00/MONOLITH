/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t *framebuffer;
    size_t width;
    size_t height;
} framebuffer_t;
