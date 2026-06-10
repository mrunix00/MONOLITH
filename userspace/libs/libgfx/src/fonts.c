/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libgfx.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void) (u), malloc(x))
#define STBTT_free(x, u) ((void) (u), free(x))
#include <stb_truetype.h>

static gfx_glyph_t *_glyph_at(gfx_font_t *font, size_t index)
{
    return (gfx_glyph_t *) (font->arena + index);
}

static bool _arena_grow(gfx_font_t *font, size_t required)
{
    size_t capacity = font->arena_capacity == 0 ? 4096 : font->arena_capacity;
    while (capacity < required)
        capacity *= 2;

    unsigned char *old_arena = font->arena;
    unsigned char *new_arena = realloc(font->arena, capacity);
    if (!new_arena)
        return false;

    font->arena = new_arena;
    font->arena_capacity = capacity;

    if (old_arena && old_arena != new_arena) {
        intptr_t delta = (intptr_t) new_arena - (intptr_t) old_arena;
        if (font->data && font->data >= old_arena && font->data < old_arena + font->arena_size)
            font->data += delta;
        for (size_t index = font->first_glyph; index != SIZE_MAX;) {
            gfx_glyph_t *glyph = _glyph_at(font, index);
            if (glyph->bitmap)
                glyph->bitmap += delta;
            index = glyph->next;
        }
        if (font->data)
            stbtt_InitFont(&font->info, font->data, font->font_offset);
    }

    return true;
}

static void *_arena_alloc(gfx_font_t *font, size_t size, size_t alignment, size_t *index)
{
    size_t offset = (font->arena_size + alignment - 1) & ~(alignment - 1);
    size_t required = offset + size;
    if (required > font->arena_capacity && !_arena_grow(font, required))
        return NULL;

    void *ptr = font->arena + offset;
    font->arena_size = required;
    if (index)
        *index = offset;
    return ptr;
}

static inline uint8_t _mul_u8(uint8_t a, uint8_t b)
{
    return (uint8_t) ((a * b + 127) / 255);
}

static uint32_t _font_line_height(gfx_font_t *font)
{
    if (!font)
        return 0;
    return font->line_height > 0 ? font->line_height : font->pixel_size;
}

static bool _font_valid(gfx_font_t *font)
{
    return font && font->data;
}

static uint32_t _font_codepoint(gfx_font_t *font, uint32_t codepoint)
{
    if (!_font_valid(font))
        return 0;
    if (codepoint == 0 || stbtt_FindGlyphIndex(&font->info, (int) codepoint) != 0)
        return codepoint;
    if (codepoint != '?' && stbtt_FindGlyphIndex(&font->info, '?') != 0)
        return '?';
    if (codepoint != ' ' && stbtt_FindGlyphIndex(&font->info, ' ') != 0)
        return ' ';
    return codepoint;
}

/*
 * Warning: This function is vibecoded and I don't understand shit from it, it works tho.
 */
static bool _decode_utf8(const char **text, uint32_t *codepoint)
{
    const unsigned char *s = (const unsigned char *) *text;
    uint32_t cp;
    size_t len;

    if (*s == '\0')
        return false;

    if (*s < 0x80) {
        cp = *s;
        len = 1;
    } else if ((*s & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((uint32_t) (s[0] & 0x1F) << 6) | (uint32_t) (s[1] & 0x3F);
        len = 2;
    } else if ((*s & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((uint32_t) (s[0] & 0x0F) << 12) | ((uint32_t) (s[1] & 0x3F) << 6)
             | (uint32_t) (s[2] & 0x3F);
        len = 3;
    } else if (
        (*s & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80
        && (s[3] & 0xC0) == 0x80) {
        cp = ((uint32_t) (s[0] & 0x07) << 18) | ((uint32_t) (s[1] & 0x3F) << 12)
             | ((uint32_t) (s[2] & 0x3F) << 6) | (uint32_t) (s[3] & 0x3F);
        len = 4;
    } else {
        cp = 0xFFFD;
        len = 1;
    }

    *codepoint = cp;
    *text += len;
    return true;
}

static gfx_glyph_t *_find_cached_glyph(gfx_font_t *font, uint32_t codepoint)
{
    if (!_font_valid(font))
        return NULL;
    for (size_t index = font->first_glyph; index != SIZE_MAX;) {
        gfx_glyph_t *glyph = _glyph_at(font, index);
        if (glyph->codepoint == codepoint)
            return glyph;
        index = glyph->next;
    }
    return NULL;
}

static gfx_glyph_t *_append_cached_glyph(gfx_font_t *font, size_t *glyph_index)
{
    size_t index;
    gfx_glyph_t *glyph = _arena_alloc(font, sizeof(*glyph), sizeof(void *), &index);
    if (!glyph)
        return NULL;

    memset(glyph, 0, sizeof(*glyph));
    glyph->next = SIZE_MAX;
    if (font->last_glyph != SIZE_MAX)
        _glyph_at(font, font->last_glyph)->next = index;
    else
        font->first_glyph = index;

    font->last_glyph = index;
    if (glyph_index)
        *glyph_index = index;
    return glyph;
}

static gfx_glyph_t *_get_glyph(gfx_font_t *font, uint32_t codepoint)
{
    if (!_font_valid(font))
        return NULL;

    uint32_t resolved = _font_codepoint(font, codepoint);
    gfx_glyph_t *glyph = _find_cached_glyph(font, resolved);
    if (glyph)
        return glyph;

    size_t glyph_index = SIZE_MAX;
    if (!(glyph = _append_cached_glyph(font, &glyph_index)))
        return NULL;

    int advance = 0;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    stbtt_GetCodepointHMetrics(&font->info, (int) resolved, &advance, NULL);
    stbtt_GetCodepointBitmapBox(
        &font->info, (int) resolved, font->scale, font->scale, &x0, &y0, &x1, &y1);

    glyph->codepoint = resolved;
    glyph->width = x1 - x0;
    glyph->height = y1 - y0;
    glyph->x_offset = x0;
    glyph->y_offset = y0;
    glyph->x_advance = (int) ((float) advance * font->scale + 0.5f);

    if (glyph->width > 0 && glyph->height > 0) {
        size_t bitmap_size = (size_t) glyph->width * (size_t) glyph->height;
        unsigned char *bitmap = _arena_alloc(font, bitmap_size, 1, NULL);
        if (!bitmap)
            return glyph;
        glyph = _glyph_at(font, glyph_index);
        glyph->bitmap = bitmap;
        stbtt_MakeCodepointBitmap(
            &font->info,
            glyph->bitmap,
            glyph->width,
            glyph->height,
            glyph->width,
            font->scale,
            font->scale,
            (int) resolved);
    }

    return glyph;
}

static bool _init_font(gfx_font_t *font, unsigned char *data, uint32_t font_size)
{
    int offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0)
        return false;

    font->data = data;
    font->font_offset = offset;
    if (stbtt_InitFont(&font->info, font->data, font->font_offset) == 0)
        return false;

    font->pixel_size = font_size;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, (float) font_size);

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    int line_height = (int) ((float) (ascent - descent + line_gap) * font->scale + 0.5f);
    font->line_height = line_height > 0 ? (uint32_t) line_height : font_size;
    return true;
}

gfx_font_t gfx_load_font(const void *data, uint32_t font_size)
{
    gfx_font_t font = {.first_glyph = SIZE_MAX, .last_glyph = SIZE_MAX};
    if (!_init_font(&font, (unsigned char *) data, font_size))
        return (gfx_font_t){0};
    return font;
}

gfx_font_t gfx_load_font_from_file(const char *path, uint32_t font_size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return (gfx_font_t){0};

    file_stats_t stats;
    if (fstat(fd, &stats) < 0 || stats.type != TYPE_FILE || stats.size == 0) {
        close(fd);
        return (gfx_font_t){0};
    }

    gfx_font_t font = {.first_glyph = SIZE_MAX, .last_glyph = SIZE_MAX};
    unsigned char *data = _arena_alloc(&font, (size_t) stats.size, sizeof(void *), NULL);
    if (!data) {
        close(fd);
        return (gfx_font_t){0};
    }

    uint64_t read_size = 0;
    while (read_size < stats.size) {
        uint32_t chunk = (uint32_t) ((stats.size - read_size) > 8192 ? 8192
                                                                     : (stats.size - read_size));
        int bytes_read = read(fd, data + read_size, chunk);
        if (bytes_read <= 0) {
            close(fd);
            gfx_unload_font(&font);
            return (gfx_font_t){0};
        }
        read_size += (uint64_t) bytes_read;
    }

    close(fd);

    if (!_init_font(&font, data, font_size)) {
        gfx_unload_font(&font);
        return (gfx_font_t){0};
    }
    return font;
}

void gfx_unload_font(gfx_font_t *font)
{
    free(font->arena);
    *font = (gfx_font_t){0};
}

void gfx_draw_char(
    gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, uint32_t codepoint)
{
    gfx_glyph_t *glyph = _get_glyph(font, codepoint);
    if (!glyph || !glyph->bitmap || glyph->width <= 0 || glyph->height <= 0)
        return;

    for (int y = 0; y < glyph->height; y++) {
        const unsigned char *row = glyph->bitmap + (size_t) y * (size_t) glyph->width;
        for (int x = 0; x < glyph->width; x++) {
            uint8_t alpha = row[x];
            if (alpha == 0)
                continue;

            int dst_x = (int) pos.x + glyph->x_offset + x;
            int dst_y = (int) pos.y + glyph->y_offset + y;
            if (dst_x < 0 || dst_y < 0)
                continue;

            gfx_color_t out = color;
            out.a = _mul_u8(alpha, color.a);
            if (out.a != 0)
                gfx_draw_pixel(ctx, (gfx_pos_t){(uint32_t) dst_x, (uint32_t) dst_y}, out);
        }
    }
}

uint32_t gfx_get_char_width(gfx_font_t *font, uint32_t codepoint)
{
    gfx_glyph_t *glyph = _get_glyph(font, codepoint);
    return glyph ? glyph->x_advance : 0;
}

uint32_t gfx_get_char_height(gfx_font_t *font, uint32_t codepoint)
{
    gfx_glyph_t *glyph = _get_glyph(font, codepoint);
    if (!glyph || glyph->height <= 0)
        return font && font->data ? _font_line_height(font) : 0;
    return (uint32_t) glyph->height;
}

void gfx_draw_text(
    gfx_context_t *ctx, gfx_font_t *font, gfx_pos_t pos, gfx_color_t color, const char *text)
{
    gfx_pos_t current_pos = pos;
    uint32_t line_height = font && font->data ? _font_line_height(font) : 0;
    const char *p = text;
    uint32_t codepoint;
    while (_decode_utf8(&p, &codepoint)) {
        if (codepoint == '\n') {
            current_pos.x = pos.x;
            current_pos.y += line_height;
            continue;
        }
        gfx_draw_char(ctx, font, current_pos, color, codepoint);
        current_pos.x += gfx_get_char_width(font, codepoint);
    }
}

uint32_t gfx_get_text_width(gfx_font_t *font, const char *text)
{
    uint32_t width = 0;
    uint32_t line_width = 0;
    const char *p = text;
    uint32_t codepoint;
    while (_decode_utf8(&p, &codepoint)) {
        if (codepoint == '\n') {
            width = line_width > width ? line_width : width;
            line_width = 0;
            continue;
        }
        line_width += gfx_get_char_width(font, codepoint);
    }
    return line_width > width ? line_width : width;
}

uint32_t gfx_get_text_height(gfx_font_t *font, const char *text)
{
    uint32_t lines = 1;
    const char *p = text;
    uint32_t codepoint;
    while (_decode_utf8(&p, &codepoint)) {
        if (codepoint == '\n')
            lines++;
    }
    return lines * _font_line_height(font);
}

gfx_area_t gfx_get_text_area(gfx_font_t *font, const char *text)
{
    int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int32_t pen_x = 0, pen_y = 0;
    bool has_bounds = false;
    uint32_t line_width = 0;
    uint32_t max_width = 0;
    uint32_t line_height = font && font->data ? _font_line_height(font) : 0;

    const char *p = text;
    uint32_t codepoint;
    while (_decode_utf8(&p, &codepoint)) {
        if (codepoint == '\n') {
            max_width = line_width > max_width ? line_width : max_width;
            line_width = 0;
            pen_x = 0;
            pen_y += (int32_t) line_height;
            continue;
        }

        gfx_glyph_t *glyph = _get_glyph(font, codepoint);

        if (glyph && glyph->width > 0 && glyph->height > 0) {
            int32_t glyph_left = pen_x + glyph->x_offset;
            int32_t glyph_top = pen_y + glyph->y_offset;
            int32_t glyph_right = glyph_left + glyph->width;
            int32_t glyph_bottom = glyph_top + glyph->height;

            if (!has_bounds) {
                min_x = glyph_left;
                min_y = glyph_top;
                max_x = glyph_right;
                max_y = glyph_bottom;
                has_bounds = true;
            } else {
                min_x = glyph_left < min_x ? glyph_left : min_x;
                min_y = glyph_top < min_y ? glyph_top : min_y;
                max_x = glyph_right > max_x ? glyph_right : max_x;
                max_y = glyph_bottom > max_y ? glyph_bottom : max_y;
            }
        }

        if (glyph) {
            pen_x += (int32_t) glyph->x_advance;
            line_width += glyph->x_advance;
        }
    }

    max_width = line_width > max_width ? line_width : max_width;

    uint32_t height = has_bounds && max_y > min_y ? (uint32_t) (max_y - min_y) : 0;
    return (gfx_area_t){
        .x = min_x < 0 ? (uint32_t) -min_x : 0,
        .y = min_y < 0 ? (uint32_t) -min_y : 0,
        .width = max_width,
        .height = height,
    };
}

void gfx_draw_text_centered(
    gfx_context_t *ctx, gfx_font_t *font, gfx_area_t area, gfx_color_t color, const char *text)
{
    gfx_area_t text_area = gfx_get_text_area(font, text);
    uint32_t x = area.width > text_area.width ? area.x + (area.width - text_area.width) / 2
                                              : area.x;
    uint32_t y = area.height > text_area.height ? area.y + (area.height - text_area.height) / 2
                                                : area.y;
    gfx_draw_text(ctx, font, (gfx_pos_t){x + text_area.x, y + text_area.y}, color, text);
}
