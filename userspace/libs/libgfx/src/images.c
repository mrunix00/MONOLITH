/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx/images.h>
#include <libgfx/types.h>
#include <stdlib.h>

#define FILE void
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include <stb_image.h>

#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

gfx_colored_bitmap_t gfx_load_image(void *data, size_t size)
{
    int width, height, channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels)
            free(pixels);
        return (gfx_colored_bitmap_t){0};
    }

    /* Convert RGBA (stb) to ARGB expected by the framebuffer format. */
    size_t pixel_count = (size_t) width * (size_t) height;
    for (size_t i = 0; i < pixel_count; i++) {
        unsigned char *p = pixels + i * 4;
        unsigned char tmp = p[0];
        p[0] = p[2];
        p[2] = tmp;
    }

    return (gfx_colored_bitmap_t) {.data = (uint32_t *) pixels, .width = width, .height = height};
}

void gfx_unload_image(gfx_colored_bitmap_t *bitmap)
{
    free(bitmap->data);
}

void gfx_resize_image(gfx_colored_bitmap_t *image, uint32_t width, uint32_t height)
{
    if (!image || !image->data || image->width == 0 || image->height == 0 || width == 0
        || height == 0)
        return;

    if (image->width == width && image->height == height)
        return;

    size_t new_size = (size_t) width * height * sizeof(uint32_t);
    uint32_t *resized = malloc(new_size);
    if (!resized)
        return;

    unsigned char *out_pixels = stbir_resize_uint8_linear(
        (const unsigned char *) image->data,
        (int) image->width,
        (int) image->height,
        (int) (image->width * sizeof(uint32_t)),
        (unsigned char *) resized,
        (int) width,
        (int) height,
        (int) (width * sizeof(uint32_t)),
        STBIR_ARGB);

    if (!out_pixels) {
        free(resized);
        return;
    }

    free(image->data);
    image->data = resized;
    image->width = width;
    image->height = height;
}
