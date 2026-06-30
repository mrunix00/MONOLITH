/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libui/libui.h>
#include <libui/widgets/textbox.h>
#include <string.h>

#define STB_TEXTEDIT_STRING ui_widget_t
#define STB_TEXTEDIT_STRINGLEN(obj) _textbox_string_length((obj)->string)
#define STB_TEXTEDIT_GETCHAR(obj, i) ((obj)->string[(i)])
#define STB_TEXTEDIT_NEWLINE '\n'
#define STB_TEXTEDIT_LAYOUTROW(row, obj, n) _textbox_layout_row((row), (obj), (n))
#define STB_TEXTEDIT_GETWIDTH(obj, n, i) _textbox_get_width((obj), (n), (i))
#define STB_TEXTEDIT_KEYTOTEXT(key) (((key) > 0 && (key) < 0x100) ? (key) : 0)
#define STB_TEXTEDIT_DELETECHARS(obj, pos, n) _textbox_delete_chars((obj), (pos), (n))
#define STB_TEXTEDIT_INSERTCHARS(obj, pos, chars, n) \
    _textbox_insert_chars((obj), (pos), (chars), (n))
#define STB_TEXTEDIT_K_SHIFT 0x400
#define STB_TEXTEDIT_K_LEFT 0x100
#define STB_TEXTEDIT_K_RIGHT 0x101
#define STB_TEXTEDIT_K_UP 0x102
#define STB_TEXTEDIT_K_DOWN 0x103
#define STB_TEXTEDIT_K_LINESTART 0x104
#define STB_TEXTEDIT_K_LINEEND 0x105
#define STB_TEXTEDIT_K_TEXTSTART 0x106
#define STB_TEXTEDIT_K_TEXTEND 0x107
#define STB_TEXTEDIT_K_DELETE 0x108
#define STB_TEXTEDIT_K_BACKSPACE 0x109
#define STB_TEXTEDIT_K_UNDO 0x10A
#define STB_TEXTEDIT_K_REDO 0x10B
#define STB_TEXTEDIT_K_WORDLEFT 0x10C
#define STB_TEXTEDIT_K_WORDRIGHT 0x10D
#define STB_TEXTEDIT_K_PGUP 0x10E
#define STB_TEXTEDIT_K_PGDOWN 0x10F
#define STB_TEXTEDIT_IS_SPACE(ch) ((ch) == ' ' || ((ch) >= '\t' && (ch) <= '\r'))

static int _textbox_string_length(const char *string)
{
    return string ? (int) strlen(string) : 0;
}

static void _textbox_terminate_string(char *string, size_t size)
{
    if (size > 0)
        string[size - 1] = '\0';
}

static uint32_t _textbox_char_width(ui_widget_t *widget, char ch)
{
    gfx_font_t *font = widget->theme->font;
    if (!font)
        return 0;
    return gfx_get_char_width(font, (uint32_t) (unsigned char) ch);
}

static uint32_t _textbox_text_width(ui_widget_t *widget, int start, int count)
{
    uint32_t width = 0;
    for (int i = 0; i < count && widget->string[start + i] != '\0'; i++)
        width += _textbox_char_width(widget, widget->string[start + i]);
    return width;
}

static void _textbox_layout_row(StbTexteditRow *row, ui_widget_t *widget, int start)
{
    int count = _textbox_string_length(widget->string) - start;
    if (count < 0)
        count = 0;

    row->x0 = 0.0f;
    row->x1 = (float) _textbox_text_width(widget, start, count);
    row->baseline_y_delta = (float) widget->theme->font->line_height;
    row->ymin = 0.0f;
    row->ymax = row->baseline_y_delta;
    row->num_chars = count;
}

static float _textbox_get_width(ui_widget_t *widget, int start, int char_index)
{
    int index = start + char_index;
    if (index < 0 || index >= _textbox_string_length(widget->string))
        return 0.0f;
    return (float) _textbox_char_width(widget, widget->string[index]);
}

static void _textbox_delete_chars(ui_widget_t *widget, int pos, int count)
{
    int length = _textbox_string_length(widget->string);
    if (pos < 0 || count <= 0 || pos >= length)
        return;
    if (pos + count > length)
        count = length - pos;

    memmove(widget->string + pos, widget->string + pos + count, (size_t) (length - pos - count + 1));
}

static int _textbox_insert_chars(ui_widget_t *widget, int pos, const char *chars, int count)
{
    int length = _textbox_string_length(widget->string);
    if (pos < 0 || pos > length || count <= 0 || widget->string_size == 0)
        return 0;
    if ((size_t) length + (size_t) count + 1 > widget->string_size)
        return 0;

    memmove(widget->string + pos + count, widget->string + pos, (size_t) (length - pos + 1));
    memcpy(widget->string + pos, chars, (size_t) count);
    return 1;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_TEXTEDIT_IMPLEMENTATION
#include <stb_textedit.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static uint32_t _textbox_visible_width(ui_widget_t *widget)
{
    uint32_t padding = widget->theme->inner_padding.l + widget->theme->inner_padding.r;
    return widget->computed_area.width > padding ? widget->computed_area.width - padding : 0;
}

static void _textbox_clamp(ui_widget_t *widget)
{
    int length = _textbox_string_length(widget->string);
    if (widget->edit.cursor > length)
        widget->edit.cursor = length;
    if (widget->edit.select_start > length)
        widget->edit.select_start = length;
    if (widget->edit.select_end > length)
        widget->edit.select_end = length;
}

static void _textbox_scroll_to_cursor(ui_widget_t *widget)
{
    uint32_t visible_width = _textbox_visible_width(widget);
    int length = _textbox_string_length(widget->string);
    uint32_t text_width = _textbox_text_width(widget, 0, length);
    uint32_t cursor_x = _textbox_text_width(widget, 0, widget->edit.cursor);

    if (visible_width == 0 || text_width <= visible_width) {
        widget->scroll_x = 0;
        return;
    }

    uint32_t margin = _textbox_char_width(widget, 'M');
    uint32_t max_scroll = text_width - visible_width;
    if (margin > visible_width / 2)
        margin = visible_width / 2;

    if (cursor_x < widget->scroll_x + margin)
        widget->scroll_x = cursor_x > margin ? cursor_x - margin : 0;
    else if (cursor_x + margin > widget->scroll_x + visible_width)
        widget->scroll_x = cursor_x + margin - visible_width;
    if (widget->scroll_x > max_scroll)
        widget->scroll_x = max_scroll;
}

static int _textbox_first_visible_char(ui_widget_t *widget, uint32_t *char_x)
{
    uint32_t x = 0;
    int length = _textbox_string_length(widget->string);
    for (int i = 0; i < length; i++) {
        uint32_t width = _textbox_char_width(widget, widget->string[i]);
        if (x + width > widget->scroll_x) {
            *char_x = x;
            return i;
        }
        x += width;
    }
    *char_x = x;
    return length;
}

static int _textbox_stb_key(ui_wctx_t *ctx, input_keyboard_scancode_t key)
{
    bool shift = ui_is_key_down(ctx, INPUT_KEY_LSHIFT) || ui_is_key_down(ctx, INPUT_KEY_RSHIFT);
    bool ctrl = ui_is_key_down(ctx, INPUT_KEY_LCTRL);
    int mod = shift ? STB_TEXTEDIT_K_SHIFT : 0;

    switch (key) {
    case INPUT_KEY_LEFT:
        return (ctrl ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT) | mod;
    case INPUT_KEY_RIGHT:
        return (ctrl ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT) | mod;
    case INPUT_KEY_UP:
        return STB_TEXTEDIT_K_UP | mod;
    case INPUT_KEY_DOWN:
        return STB_TEXTEDIT_K_DOWN | mod;
    case INPUT_KEY_HOME:
        return (ctrl ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_LINESTART) | mod;
    case INPUT_KEY_END:
        return (ctrl ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_LINEEND) | mod;
    case INPUT_KEY_BACKSPACE:
        return STB_TEXTEDIT_K_BACKSPACE;
    case INPUT_KEY_DELETE:
        return STB_TEXTEDIT_K_DELETE;
    case INPUT_KEY_Z:
        return ctrl ? STB_TEXTEDIT_K_UNDO : 0;
    case INPUT_KEY_Y:
        return ctrl ? STB_TEXTEDIT_K_REDO : 0;
    default:
        return 0;
    }
}

static void _textbox_insert_text(ui_widget_t *widget, const char *text)
{
    char ascii[BUI_TEXT_INPUT_MAX];
    int count = 0;
    for (const unsigned char *ch = (const unsigned char *) text; *ch && count < (int) sizeof(ascii);
         ch++) {
        if (*ch >= 0x20 && *ch < 0x7F)
            ascii[count++] = (char) *ch;
    }
    if (count > 0)
        stb_textedit_paste(widget, &widget->edit, ascii, count);
}

static float _textbox_mouse_x(ui_wctx_t *ctx, ui_widget_t *widget)
{
    int32_t text_x = (int32_t) widget->computed_area.x + (int32_t) widget->theme->inner_padding.l;
    int32_t mouse_x = (int32_t) ctx->mouse_state.pos_x;
    float local_x = mouse_x > text_x ? (float) (mouse_x - text_x) : 0.0f;
    return local_x + (float) widget->scroll_x;
}

static bool _textbox_submitted(ui_wctx_t *ctx, ui_id_t id)
{
    return ctx->focused_widget_id == id && ctx->input_event_type == BUI_EVENT_KEY_DOWN
           && ctx->input_event.data.keyboard.keyboard.scancode == INPUT_KEY_ENTER;
}

static void _textbox_handle_input(ui_wctx_t *ctx, ui_widget_t *widget)
{
    ui_event_type_t type = ctx->input_event_type;
    bool clicked = type == BUI_EVENT_MOUSE_BUTTON_DOWN && ctx->event_widget_id == widget->id
                   && (ctx->event_widget_state & BUI_WIDGET_EVENT_LCLICKED);
    bool active_click = ctx->active_widget_id == widget->id
                        && ui_is_mouse_button_down(ctx, INPUT_MOUSE_BUTTON_LEFT)
                        && ui_is_mouse_in_area(ctx, widget->computed_area);

    if (clicked || (active_click && ctx->focused_widget_id != widget->id)) {
        ctx->focused_widget_id = widget->id;
        stb_textedit_click(widget, &widget->edit, _textbox_mouse_x(ctx, widget), 0.0f);
    } else if (
        type == BUI_EVENT_MOUSE_MOVE && ctx->active_widget_id == widget->id
        && ui_is_mouse_button_down(ctx, INPUT_MOUSE_BUTTON_LEFT)) {
        stb_textedit_drag(widget, &widget->edit, _textbox_mouse_x(ctx, widget), 0.0f);
    }

    if (ctx->focused_widget_id != widget->id)
        return;

    if (type == BUI_EVENT_KEY_DOWN || type == BUI_EVENT_KEY_HOLD) {
        bool ctrl = ui_is_key_down(ctx, INPUT_KEY_LCTRL);
        if (ctrl && ctx->input_event.data.keyboard.keyboard.scancode == INPUT_KEY_A) {
            int length = _textbox_string_length(widget->string);
            widget->edit.cursor = length;
            widget->edit.select_start = 0;
            widget->edit.select_end = length;
        } else {
            int key = _textbox_stb_key(ctx, ctx->input_event.data.keyboard.keyboard.scancode);
            if (key != 0) {
                stb_textedit_key(widget, &widget->edit, key);
            } else if (type == BUI_EVENT_KEY_DOWN) {
                char ch = ui_char_from_key_scancode(
                    ctx, ctx->input_event.data.keyboard.keyboard.scancode);
                if (ch != '\0')
                    _textbox_insert_text(widget, (char[]){ch, '\0'});
            }
        }
    }

    _textbox_clamp(widget);
    _textbox_scroll_to_cursor(widget);
}

static void _draw_textbox(ui_wctx_t *ctx, ui_widget_t *widget)
{
    _textbox_handle_input(ctx, widget);
    _textbox_scroll_to_cursor(widget);
    bool focused = ctx->focused_widget_id == widget->id;
    gfx_color_t border_color = focused ? widget->theme->foreground_color
                                       : widget->theme->border_color;
    gfx_color_t fill_color = ui_is_mouse_in_area(ctx, widget->computed_area)
                                 ? (gfx_color_t){0xFF, 0x30, 0x30, 0x30}
                                 : widget->theme->background_color;

    gfx_draw_filled_rect(
        &ctx->gfx_context,
        (gfx_rect_t){
            .x = widget->computed_area.x,
            .y = widget->computed_area.y,
            .width = widget->computed_area.width,
            .height = widget->computed_area.height,
            .border_thickness = widget->theme->border_thickness,
            .border_color = border_color,
        },
        fill_color);
    uint32_t border = widget->theme->border_thickness;
    if (widget->computed_area.width > border * 2 && widget->computed_area.height > border * 2) {
        gfx_draw_rect(
            &ctx->gfx_context,
            (gfx_rect_t){
                .x = widget->computed_area.x + border,
                .y = widget->computed_area.y + border,
                .width = widget->computed_area.width - border * 2,
                .height = widget->computed_area.height - border * 2,
                .border_thickness = widget->theme->shadow_thickness,
                .border_color = widget->theme->shadow_color,
            });
    }

    gfx_area_t text_area = gfx_get_text_area(widget->theme->font, "M");
    if (text_area.height == 0)
        text_area.height = gfx_get_text_height(widget->theme->font, "M");

    uint32_t text_x = widget->computed_area.x + widget->theme->inner_padding.l;
    uint32_t text_y = widget->computed_area.height > text_area.height
                          ? widget->computed_area.y
                                + (widget->computed_area.height - text_area.height) / 2
                          : widget->computed_area.y;
    uint32_t text_draw_y = text_y + text_area.y;
    uint32_t text_bottom = text_y + text_area.height;

    gfx_area_t clip = {
        .x = text_x,
        .y = widget->computed_area.y + widget->theme->border_thickness,
        .width = _textbox_visible_width(widget),
        .height = widget->computed_area.height > widget->theme->border_thickness * 2
                      ? widget->computed_area.height - widget->theme->border_thickness * 2
                      : 0,
    };
    gfx_set_clip(&ctx->gfx_context, clip);

    if (widget->edit.select_start != widget->edit.select_end) {
        int sel_start = widget->edit.select_start < widget->edit.select_end
                            ? widget->edit.select_start
                            : widget->edit.select_end;
        int sel_end = widget->edit.select_start > widget->edit.select_end
                          ? widget->edit.select_start
                          : widget->edit.select_end;
        int32_t x0 = (int32_t) text_x + (int32_t) _textbox_text_width(widget, 0, sel_start)
                     - (int32_t) widget->scroll_x;
        int32_t x1 = x0 + (int32_t) _textbox_text_width(widget, sel_start, sel_end - sel_start);
        int32_t clip_x0 = (int32_t) clip.x;
        int32_t clip_x1 = (int32_t) (clip.x + clip.width);
        if (x0 < clip_x0)
            x0 = clip_x0;
        if (x1 > clip_x1)
            x1 = clip_x1;
        if (x1 > x0) {
            gfx_draw_filled_rect(
                &ctx->gfx_context,
                (gfx_rect_t){
                    .x = (uint32_t) x0,
                    .y = text_y,
                    .width = (uint32_t) (x1 - x0),
                    .height = text_bottom - text_y,
                },
                widget->theme->shadow_color);
        }
    }

    if (widget->string && widget->string[0] != '\0') {
        uint32_t first_char_x = 0;
        int first_char = _textbox_first_visible_char(widget, &first_char_x);
        uint32_t hidden_px = widget->scroll_x > first_char_x ? widget->scroll_x - first_char_x : 0;
        gfx_draw_text(
            &ctx->gfx_context,
            widget->theme->font,
            (gfx_pos_t){
                .x = text_x > hidden_px ? text_x - hidden_px : text_x,
                .y = text_draw_y,
            },
            widget->theme->foreground_color,
            widget->string + first_char);
    }

    if (focused) {
        int32_t cursor_x = (int32_t) text_x
                           + (int32_t) _textbox_text_width(widget, 0, widget->edit.cursor)
                           - (int32_t) widget->scroll_x;
        int32_t clip_x0 = (int32_t) clip.x;
        int32_t clip_x1 = (int32_t) (clip.x + clip.width);
        if (cursor_x < clip_x0)
            cursor_x = clip_x0;
        if (cursor_x >= clip_x1)
            cursor_x = clip_x1 > clip_x0 ? clip_x1 - 1 : clip_x0;

        int32_t cursor_y1 = (int32_t) text_y - 2;
        int32_t cursor_y2 = (int32_t) text_bottom + 2;
        int32_t clip_y0 = (int32_t) clip.y;
        int32_t clip_y1 = (int32_t) (clip.y + clip.height);
        if (cursor_y1 < clip_y0)
            cursor_y1 = clip_y0;
        if (cursor_y2 > clip_y1)
            cursor_y2 = clip_y1;

        gfx_draw_line(
            &ctx->gfx_context,
            (gfx_line_t){
                .x1 = (uint32_t) cursor_x,
                .x2 = (uint32_t) cursor_x,
                .y1 = (uint32_t) cursor_y1,
                .y2 = (uint32_t) cursor_y2,
                .thickness = 1,
            },
            widget->theme->foreground_color);
    }

    gfx_reset_clip(&ctx->gfx_context);
}

bool ui_textbox(ui_wctx_t *wctx, const char *label, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
        return false;

    ui_id_t id = ui_hash_string(label ? label : "");
    ui_theme_t *theme = ui_get_current_theme(wctx);
    bool submitted = _textbox_submitted(wctx, id);
    _textbox_terminate_string(buffer, buffer_size);

    if (!wctx->hot_state) {
        ui_widget_t widget = {
            .flags = BUI_WIDGET_FLAG_CLICKABLE,
            .label = label,
            .string = buffer,
            .string_size = buffer_size,
            .theme = theme,
            .semantic_size = {
                [BUI_AXIS_X] = {
                    .type = BUI_WIDGET_SIZE_TYPE_PERCENTAGE,
                    .value = 1.0f,
                    .strictness = 0.0f,
                },
                [BUI_AXIS_Y] = {
                    .type = BUI_WIDGET_SIZE_TYPE_FIXED,
                    .value = gfx_get_text_height(theme->font, "M") + theme->inner_padding.t
                             + theme->inner_padding.b,
                    .strictness = 0.0f,
                },
            },
            .draw = _draw_textbox,
        };
        stb_textedit_initialize_state(&widget.edit, 1);
        ui_widget_t *textbox = ui_push_new_child(wctx, &widget);
        if (textbox)
            ui_push_id(wctx, id, textbox);
        return submitted;
    }

    ui_widget_t *textbox = ui_get_by_id(wctx, id);
    if (textbox == NULL)
        return submitted;
    textbox->label = label;
    textbox->string = buffer;
    textbox->string_size = buffer_size;
    _textbox_terminate_string(buffer, buffer_size);
    return submitted;
}
