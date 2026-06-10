/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/framebuffer.h>
#include <stdint.h>

static framebuffer_t _framebuffers[MAX_FRAMEBUFFERS];
static uint8_t _framebuffer_count = 0;

void setup_framebuffer(framebuffer_t fb)
{
    if (_framebuffer_count < MAX_FRAMEBUFFERS)
        _framebuffers[_framebuffer_count++] = fb;
}

framebuffer_t *get_framebuffer(uint8_t index)
{
    return index >= _framebuffer_count ? NULL : &_framebuffers[index];
}
