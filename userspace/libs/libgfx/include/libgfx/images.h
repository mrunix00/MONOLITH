/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libgfx/types.h>
#include <stdint.h>

gfx_colored_bitmap_t gfx_load_image(void *data, size_t size);

void gfx_unload_image(gfx_colored_bitmap_t *bitmap);

void gfx_resize_image(gfx_colored_bitmap_t *image, uint32_t width, uint32_t height);
