/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <debug.h>
#include <libdesktop.h>
#include <libgfx.h>
#include <resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

#define TERM_TRANSCRIPT_LEN 8192
#define TERM_MARGIN 8
#define TERM_FONT_SIZE 20
#define TERM_CSI_PARAM_COUNT 8
#define TERM_FONT_PATH "file:/system/assets/JetBrainsMono-Medium.ttf"

enum {
    SHELL_TIN = TERM_RD_TIN,
    SHELL_TOUT = TERM_RD_TOUT,
    SHELL_RD_COUNT = 2,
};

typedef enum {
    TERM_PARSE_NORMAL = 0,
    TERM_PARSE_ESCAPE,
    TERM_PARSE_CSI,
} terminal_parse_state_t;

typedef struct
{
    char ch;
    gfx_color_t fg;
    gfx_color_t bg;
} terminal_cell_t;

typedef struct
{
    gfx_font_t font;
    terminal_cell_t *cells;
    char transcript[TERM_TRANSCRIPT_LEN];
    uint32_t transcript_len;
    uint16_t cell_width;
    uint16_t cell_height;
    uint16_t baseline;
    uint16_t cols;
    uint16_t rows;
    uint16_t cursor_col;
    uint16_t cursor_row;
    uint16_t saved_col;
    uint16_t saved_row;
    gfx_color_t fg;
    gfx_color_t bg;
    terminal_parse_state_t parse_state;
    uint16_t csi_params[TERM_CSI_PARAM_COUNT];
    uint8_t csi_param_count;
    uint16_t csi_current;
    bool csi_has_current;
    bool shift_down;
    bool caps_lock;
    bool dirty;
    rsrc_handle_t shell_rds[SHELL_RD_COUNT];
    rsrc_handle_t shell_task;
} terminal_state_t;

typedef struct
{
    uint8_t scancode;
    const char *sequence;
    uint32_t length;
} key_sequence_t;

static const gfx_color_t TERM_DEFAULT_BG = {0xFF, 0x10, 0x13, 0x17};
static const gfx_color_t TERM_DEFAULT_FG = {0xFF, 0xD7, 0xDE, 0xE8};
static const gfx_color_t TERM_PALETTE[8] = {
    {0xFF, 0x10, 0x13, 0x17},
    {0xFF, 0xD8, 0x58, 0x58},
    {0xFF, 0x62, 0xC4, 0x73},
    {0xFF, 0xE3, 0xB3, 0x41},
    {0xFF, 0x58, 0x91, 0xD8},
    {0xFF, 0xB4, 0x75, 0xD8},
    {0xFF, 0x5F, 0xC7, 0xD8},
    {0xFF, 0xD7, 0xDE, 0xE8},
};

static uint16_t _term_clamp_u16(uint16_t value, uint16_t max)
{
    return value > max ? max : value;
}

static terminal_cell_t *_term_cell_at(terminal_state_t *term, uint16_t col, uint16_t row)
{
    return &term->cells[(size_t) row * term->cols + col];
}

static void _term_record_bytes(terminal_state_t *term, const char *text, size_t len)
{
    if (!term || !text || len == 0)
        return;

    if (len >= TERM_TRANSCRIPT_LEN) {
        text += len - (TERM_TRANSCRIPT_LEN - 1);
        len = TERM_TRANSCRIPT_LEN - 1;
        term->transcript_len = 0;
    } else if (term->transcript_len + len >= TERM_TRANSCRIPT_LEN) {
        uint32_t drop = (uint32_t) (term->transcript_len + len - (TERM_TRANSCRIPT_LEN - 1));
        memmove(term->transcript, term->transcript + drop, term->transcript_len - drop);
        term->transcript_len -= drop;
    }

    memcpy(term->transcript + term->transcript_len, text, len);
    term->transcript_len += (uint32_t) len;
    term->transcript[term->transcript_len] = '\0';
}

static void _term_record(terminal_state_t *term, const char *text)
{
    if (!text)
        return;
    _term_record_bytes(term, text, strlen(text));
}

static void _term_mark_dirty(terminal_state_t *term)
{
    if (term)
        term->dirty = true;
}

static void _term_reset_parser(terminal_state_t *term)
{
    term->parse_state = TERM_PARSE_NORMAL;
    term->csi_param_count = 0;
    term->csi_current = 0;
    term->csi_has_current = false;
}

static void _term_clear_cells(terminal_state_t *term)
{
    if (!term || !term->cells)
        return;

    for (uint32_t i = 0; i < (uint32_t) term->cols * term->rows; i++) {
        term->cells[i].ch = ' ';
        term->cells[i].fg = term->fg;
        term->cells[i].bg = term->bg;
    }
}

static void _term_reset_screen(terminal_state_t *term)
{
    term->cursor_col = 0;
    term->cursor_row = 0;
    term->saved_col = 0;
    term->saved_row = 0;
    term->fg = TERM_DEFAULT_FG;
    term->bg = TERM_DEFAULT_BG;
    _term_reset_parser(term);
    _term_clear_cells(term);
    _term_mark_dirty(term);
}

static void _term_scroll(terminal_state_t *term)
{
    if (!term || !term->cells || term->rows == 0)
        return;

    if (term->rows > 1) {
        memmove(
            term->cells,
            term->cells + term->cols,
            (size_t) (term->rows - 1) * term->cols * sizeof(*term->cells));
    }

    for (uint16_t col = 0; col < term->cols; col++) {
        terminal_cell_t *cell = _term_cell_at(term, col, term->rows - 1);
        cell->ch = ' ';
        cell->fg = term->fg;
        cell->bg = term->bg;
    }
}

static void _term_newline(terminal_state_t *term)
{
    if (term->cursor_row + 1 >= term->rows) {
        _term_scroll(term);
    } else {
        term->cursor_row++;
    }
}

static void _term_put_char(terminal_state_t *term, char ch)
{
    if (!term || !term->cells || term->cols == 0 || term->rows == 0)
        return;

    if (ch == '\r') {
        term->cursor_col = 0;
        return;
    }
    if (ch == '\n') {
        _term_newline(term);
        return;
    }
    if (ch == '\b' || ch == 0x7f) {
        if (term->cursor_col > 0)
            term->cursor_col--;
        return;
    }
    if (ch == '\t') {
        uint16_t next = (uint16_t) ((term->cursor_col + 8) & ~7u);
        term->cursor_col = next >= term->cols ? (uint16_t) (term->cols - 1) : next;
        return;
    }
    if ((unsigned char) ch < 0x20)
        return;

    terminal_cell_t *cell = _term_cell_at(term, term->cursor_col, term->cursor_row);
    cell->ch = ch;
    cell->fg = term->fg;
    cell->bg = term->bg;

    if (term->cursor_col + 1 >= term->cols) {
        term->cursor_col = 0;
        _term_newline(term);
    } else {
        term->cursor_col++;
    }
}

static void _term_csi_push_current(terminal_state_t *term)
{
    if (term->csi_param_count >= TERM_CSI_PARAM_COUNT)
        return;

    term->csi_params[term->csi_param_count++] = term->csi_has_current ? term->csi_current : 0;
    term->csi_current = 0;
    term->csi_has_current = false;
}

static uint16_t _term_csi_param(terminal_state_t *term, uint8_t index, uint16_t fallback)
{
    if (index >= term->csi_param_count)
        return fallback;
    return term->csi_params[index] == 0 ? fallback : term->csi_params[index];
}

static void _term_clear_from_cursor(terminal_state_t *term)
{
    for (uint16_t row = term->cursor_row; row < term->rows; row++) {
        uint16_t start = row == term->cursor_row ? term->cursor_col : 0;
        for (uint16_t col = start; col < term->cols; col++) {
            terminal_cell_t *cell = _term_cell_at(term, col, row);
            cell->ch = ' ';
            cell->fg = term->fg;
            cell->bg = term->bg;
        }
    }
}

static void _term_clear_line_from_cursor(terminal_state_t *term)
{
    for (uint16_t col = term->cursor_col; col < term->cols; col++) {
        terminal_cell_t *cell = _term_cell_at(term, col, term->cursor_row);
        cell->ch = ' ';
        cell->fg = term->fg;
        cell->bg = term->bg;
    }
}

static void _term_apply_sgr(terminal_state_t *term)
{
    if (term->csi_param_count == 0) {
        term->fg = TERM_DEFAULT_FG;
        term->bg = TERM_DEFAULT_BG;
        return;
    }

    for (uint8_t i = 0; i < term->csi_param_count; i++) {
        uint16_t param = term->csi_params[i];
        if (param == 0) {
            term->fg = TERM_DEFAULT_FG;
            term->bg = TERM_DEFAULT_BG;
        } else if (param >= 30 && param <= 37) {
            term->fg = TERM_PALETTE[param - 30];
        } else if (param == 39) {
            term->fg = TERM_DEFAULT_FG;
        } else if (param >= 40 && param <= 47) {
            term->bg = TERM_PALETTE[param - 40];
        } else if (param == 49) {
            term->bg = TERM_DEFAULT_BG;
        }
    }
}

static void _term_handle_csi(terminal_state_t *term, char final)
{
    if (term->csi_has_current || term->csi_param_count == 0)
        _term_csi_push_current(term);

    switch (final) {
    case 'A': {
        uint16_t n = _term_csi_param(term, 0, 1);
        term->cursor_row = n > term->cursor_row ? 0 : (uint16_t) (term->cursor_row - n);
        break;
    }
    case 'B':
        term->cursor_row = _term_clamp_u16(
            (uint16_t) (term->cursor_row + _term_csi_param(term, 0, 1)), term->rows - 1);
        break;
    case 'C':
        term->cursor_col = _term_clamp_u16(
            (uint16_t) (term->cursor_col + _term_csi_param(term, 0, 1)), term->cols - 1);
        break;
    case 'D': {
        uint16_t n = _term_csi_param(term, 0, 1);
        term->cursor_col = n > term->cursor_col ? 0 : (uint16_t) (term->cursor_col - n);
        break;
    }
    case 'H':
    case 'f': {
        uint16_t row = _term_csi_param(term, 0, 1);
        uint16_t col = _term_csi_param(term, 1, 1);
        term->cursor_row = _term_clamp_u16((uint16_t) (row - 1), term->rows - 1);
        term->cursor_col = _term_clamp_u16((uint16_t) (col - 1), term->cols - 1);
        break;
    }
    case 'J':
        if (term->csi_params[0] == 2) {
            _term_clear_cells(term);
            term->cursor_col = 0;
            term->cursor_row = 0;
        } else {
            _term_clear_from_cursor(term);
        }
        break;
    case 'K':
        _term_clear_line_from_cursor(term);
        break;
    case 'm':
        _term_apply_sgr(term);
        break;
    case 's':
        term->saved_col = term->cursor_col;
        term->saved_row = term->cursor_row;
        break;
    case 'u':
        term->cursor_col = _term_clamp_u16(term->saved_col, term->cols - 1);
        term->cursor_row = _term_clamp_u16(term->saved_row, term->rows - 1);
        break;
    default:
        break;
    }

    _term_reset_parser(term);
}

static void _term_process_byte(terminal_state_t *term, char ch)
{
    switch (term->parse_state) {
    case TERM_PARSE_NORMAL:
        if (ch == '\x1b') {
            term->parse_state = TERM_PARSE_ESCAPE;
            return;
        }
        _term_put_char(term, ch);
        return;
    case TERM_PARSE_ESCAPE:
        if (ch == '[') {
            term->parse_state = TERM_PARSE_CSI;
            term->csi_param_count = 0;
            term->csi_current = 0;
            term->csi_has_current = false;
            return;
        }
        if (ch == '7') {
            term->saved_col = term->cursor_col;
            term->saved_row = term->cursor_row;
        } else if (ch == '8') {
            term->cursor_col = _term_clamp_u16(term->saved_col, term->cols - 1);
            term->cursor_row = _term_clamp_u16(term->saved_row, term->rows - 1);
        }
        _term_reset_parser(term);
        return;
    case TERM_PARSE_CSI:
        if (ch >= '0' && ch <= '9') {
            term->csi_current = (uint16_t) (term->csi_current * 10 + (ch - '0'));
            term->csi_has_current = true;
            return;
        }
        if (ch == ';') {
            _term_csi_push_current(term);
            return;
        }
        if (ch == '?' || ch == ' ')
            return;
        _term_handle_csi(term, ch);
        return;
    }
}

static void _term_emit_bytes(terminal_state_t *term, const char *text, size_t len)
{
    if (!term || !term->cells || !text || len == 0)
        return;

    for (size_t i = 0; i < len; i++)
        _term_process_byte(term, text[i]);
    _term_mark_dirty(term);
}

static void _term_write_bytes(terminal_state_t *term, const char *text, size_t len)
{
    _term_record_bytes(term, text, len);
    _term_emit_bytes(term, text, len);
}

static void _redraw_terminal(terminal_state_t *term)
{
    if (!term || !term->cells)
        return;

    _term_reset_screen(term);
    if (term->transcript_len > 0)
        _term_emit_bytes(term, term->transcript, term->transcript_len);
}

static bool _term_init_font(terminal_state_t *term)
{
    if (!term || term->font.data)
        return term && term->font.data;

    term->font = gfx_load_font_from_file(TERM_FONT_PATH, TERM_FONT_SIZE);
    if (!term->font.data) {
        debug_log("terminal: failed to load font\n");
        return false;
    }

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&term->font.info, &ascent, &descent, &line_gap);

    uint32_t cell_width = gfx_get_char_width(&term->font, 'M');
    uint32_t cell_height = term->font.line_height;
    int baseline = (int) ((float) ascent * term->font.scale + 0.5f);

    term->cell_width = cell_width > 0 ? (uint16_t) cell_width : TERM_FONT_SIZE / 2;
    term->cell_height = cell_height > 0 ? (uint16_t) cell_height : TERM_FONT_SIZE;
    term->baseline = baseline > 0 ? (uint16_t) baseline : term->cell_height;
    return true;
}

static void _term_destroy_screen(terminal_state_t *term)
{
    if (!term)
        return;

    free(term->cells);
    term->cells = NULL;
    term->cols = 0;
    term->rows = 0;
    term->dirty = false;
}

static bool _term_resize_screen(terminal_state_t *term, gfx_context_t *ctx)
{
    if (!term || !ctx || !ctx->framebuffer)
        return false;

    if (!_term_init_font(term))
        return false;

    if (ctx->width <= TERM_MARGIN * 2 || ctx->height <= TERM_MARGIN * 2)
        return false;

    uint16_t cols = (uint16_t) ((ctx->width - TERM_MARGIN * 2) / term->cell_width);
    uint16_t rows = (uint16_t) ((ctx->height - TERM_MARGIN * 2) / term->cell_height);
    if (cols == 0 || rows == 0)
        return false;

    terminal_cell_t *cells = calloc((size_t) cols * rows, sizeof(*cells));
    if (!cells)
        return false;

    free(term->cells);
    term->cells = cells;
    term->cols = cols;
    term->rows = rows;
    _redraw_terminal(term);
    return true;
}

static void _term_deinit(terminal_state_t *term)
{
    if (!term)
        return;

    _term_destroy_screen(term);
    if (term->font.data)
        gfx_unload_font(&term->font);
    if (term->shell_task >= 0) {
        close(term->shell_task);
        term->shell_task = -1;
    }
}

static void _term_render(terminal_state_t *term, gfx_context_t *ctx)
{
    if (!term || !ctx || !ctx->backbuffer || !term->cells)
        return;

    gfx_begin_frame(ctx);
    gfx_clear(ctx, TERM_DEFAULT_BG);

    for (uint16_t row = 0; row < term->rows; row++) {
        for (uint16_t col = 0; col < term->cols; col++) {
            terminal_cell_t *cell = _term_cell_at(term, col, row);
            bool cursor = row == term->cursor_row && col == term->cursor_col;
            gfx_color_t fg = cursor ? cell->bg : cell->fg;
            gfx_color_t bg = cursor ? cell->fg : cell->bg;
            uint32_t x = TERM_MARGIN + (uint32_t) col * term->cell_width;
            uint32_t y = TERM_MARGIN + (uint32_t) row * term->cell_height;

            gfx_draw_filled_rect(
                ctx,
                (gfx_rect_t){
                    .x = x,
                    .y = y,
                    .width = term->cell_width,
                    .height = term->cell_height,
                },
                bg);
            if (cell->ch != ' ')
                gfx_draw_char(
                    ctx, &term->font, (gfx_pos_t){x, y + term->baseline}, fg, (uint32_t) cell->ch);
        }
    }

    gfx_end_frame(ctx);
}

static bool _term_send_event(
    terminal_state_t *term, term_event_type_t type, const void *payload, uint32_t length)
{
    if (!term || term->shell_rds[SHELL_TOUT] < 0 || length > TERM_MAX_PAYLOAD)
        return false;

    return term_write_event(term->shell_rds[SHELL_TOUT], type, payload, length) == 0;
}

static void _term_send_dimensions_event(terminal_state_t *term, term_event_type_t type)
{
    if (!term || !term->cells || term->shell_rds[SHELL_TOUT] < 0)
        return;

    term_dimensions_t dimensions = {.rows = term->rows, .cols = term->cols};
    _term_send_event(term, type, &dimensions, sizeof(dimensions));
}

static void _pump_shell_commands(terminal_state_t *term)
{
    if (!term || term->shell_rds[SHELL_TIN] < 0)
        return;

    while (1) {
        char payload[TERM_MAX_PAYLOAD];
        term_command_t command = {0};
        int result
            = term_read_command(term->shell_rds[SHELL_TIN], &command, payload, sizeof(payload));
        if (result == 0)
            break;
        if (result < 0) {
            debug_log("terminal: invalid term command packet\n");
            break;
        }

        switch (command.type) {
        case TERM_COMMAND_WRITE_VT100:
            _term_write_bytes(term, payload, command.length);
            break;
        case TERM_COMMAND_GET_TERM_INFO:
            _term_send_dimensions_event(term, TERM_EVENT_WINDOW_INFO);
            break;
        default:
            debug_log("terminal: invalid term command\n");
            break;
        }
    }
}

static bool _wait_for_terminal_activity(terminal_state_t *term, rsrc_handle_t desktop_handle)
{
    rsrc_poll_t polls[3];
    uint32_t count = 0;
    uint32_t shell_task_index = UINT32_MAX;

    if (desktop_handle >= 0)
        polls[count++] = (rsrc_poll_t){.handle = desktop_handle, .events = RSRC_POLL_READ};
    if (term && term->shell_rds[SHELL_TIN] >= 0)
        polls[count++]
            = (rsrc_poll_t){.handle = term->shell_rds[SHELL_TIN], .events = RSRC_POLL_READ};
    if (term && term->shell_task >= 0) {
        shell_task_index = count;
        polls[count++] = (rsrc_poll_t){.handle = term->shell_task, .events = RSRC_POLL_READ};
    }

    if (count == 0)
        return false;

    return rsmgr_poll(polls, count) == (int) shell_task_index;
}

static bool _create_shell_rds(terminal_state_t *term)
{
    for (uint32_t i = 0; i < SHELL_RD_COUNT; i++) {
        term->shell_rds[i] = pipe_create(NULL);
        if (term->shell_rds[i] < 0) {
            _term_record(term, "terminal: failed to create shell rds\r\n");
            return false;
        }
    }

    return true;
}

static bool _launch_shell(terminal_state_t *term)
{
    if (!term)
        return false;

    if (!_create_shell_rds(term))
        return false;

    task_create_inherit_t inherit[SHELL_RD_COUNT];
    for (uint32_t rd = 0; rd < SHELL_RD_COUNT; rd++) {
        inherit[rd].current_descriptor = term->shell_rds[rd];
        inherit[rd].target_descriptor = (int) rd;
    }

    const char *argv[] = {"file:/system/shell"};
    term->shell_task = task_create(1, argv, inherit, SHELL_RD_COUNT);
    if (term->shell_task < 0) {
        _term_record(term, "terminal: failed to launch file:/system/shell\r\n");
        return false;
    }

    return true;
}

static char _scancode_to_char(uint8_t scancode, bool shift, bool caps_lock)
{
    static const char normal[128] = {
        [0x02] = '1',  [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',  [0x07] = '6',
        [0x08] = '7',  [0x09] = '8', [0x0A] = '9', [0x0B] = '0', [0x0C] = '-',  [0x0D] = '=',
        [0x10] = 'q',  [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',  [0x15] = 'y',
        [0x16] = 'u',  [0x17] = 'i', [0x18] = 'o', [0x19] = 'p', [0x1A] = '[',  [0x1B] = ']',
        [0x1E] = 'a',  [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',  [0x23] = 'h',
        [0x24] = 'j',  [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
        [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',  [0x30] = 'b',
        [0x31] = 'n',  [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',  [0x39] = ' ',
    };
    static const char shifted[128] = {
        [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^',
        [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
        [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|',
        [0x33] = '<', [0x34] = '>', [0x35] = '?',
    };

    char ch = normal[scancode];
    if (ch >= 'a' && ch <= 'z') {
        if (shift != caps_lock)
            ch = (char) (ch - 'a' + 'A');
        return ch;
    }

    if (shift && shifted[scancode] != '\0')
        return shifted[scancode];

    return ch;
}

static void _handle_keyboard_event(terminal_state_t *term, const window_keyboard_event_t *event)
{
    if (!term || !event)
        return;

    uint8_t scancode = event->keyboard.scancode;
    uint8_t action = event->keyboard.action;

    if (scancode == 0x2A || scancode == 0x36) {
        term->shift_down = action != INPUT_KEYBOARD_ACTION_RELEASED;
        return;
    }

    if (action == INPUT_KEYBOARD_ACTION_RELEASED)
        return;

    if (scancode == 0x3A) {
        if (action == INPUT_KEYBOARD_ACTION_PRESSED)
            term->caps_lock = !term->caps_lock;
        return;
    }

    if (term->shell_rds[SHELL_TOUT] < 0)
        return;

    static const key_sequence_t special_keys[] = {
        {0x0E, "\x7f", 1},
        {0x1C, "\r", 1},
        {0x48, "\x1b[A", 3},
        {0x50, "\x1b[B", 3},
        {0x4D, "\x1b[C", 3},
        {0x4B, "\x1b[D", 3},
    };

    for (uint32_t i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); i++) {
        if (special_keys[i].scancode != scancode)
            continue;
        _term_send_event(
            term, TERM_EVENT_INPUT_VT100, special_keys[i].sequence, special_keys[i].length);
        return;
    }

    char ch = _scancode_to_char(scancode, term->shift_down, term->caps_lock);
    if (ch == '\0')
        return;

    _term_send_event(term, TERM_EVENT_INPUT_VT100, &ch, 1);
}

static void _present_if_dirty(
    bool framebuffer_ready, uint32_t window_id, gfx_context_t *ctx, terminal_state_t *term)
{
    if (!framebuffer_ready || !ctx || !term || !term->dirty)
        return;

    _term_render(term, ctx);
    desktop_present_window((uint16_t) window_id);
    gfx_wait_frame(ctx);
    term->dirty = false;
}

static void _resize_terminal(
    terminal_state_t *term,
    gfx_context_t *ctx,
    uint32_t window_id,
    term_event_type_t dimensions_event,
    bool *framebuffer_ready)
{
    _term_destroy_screen(term);
    bool term_ready = _term_resize_screen(term, ctx);
    *framebuffer_ready = ctx && ctx->framebuffer && ctx->width > 0 && ctx->height > 0;
    if (term_ready)
        _term_send_dimensions_event(term, dimensions_event);
    term->dirty = *framebuffer_ready;
    _present_if_dirty(*framebuffer_ready, window_id, ctx, term);
}

int main(void)
{
    gfx_context_t gfx_context = {0};
    bool created = false;
    bool create_pending = false;
    bool framebuffer_ready = false;
    term_event_type_t next_dimensions_event = TERM_EVENT_WINDOW_INFO;
    uint32_t create_sequence = 0;
    uint32_t window_id = 0;
    rsrc_handle_t desktop_handle;
    terminal_state_t terminal = {
        .shell_rds = {[SHELL_TIN] = -1, [SHELL_TOUT] = -1},
        .shell_task = -1,
        .fg = TERM_DEFAULT_FG,
        .bg = TERM_DEFAULT_BG,
    };

    if (!_launch_shell(&terminal))
        debug_log("terminal: failed to launch shell\n");

    desktop_handle = desktop_connect();
    if (desktop_handle < 0) {
        debug_log("failed to connect to desktop\n");
        _term_deinit(&terminal);
        return 1;
    }

    while (1) {
        if (!created && !create_pending) {
            create_sequence = desktop_create_window(
                640,
                480,
                (window_flags_t){
                    .borderless = false,
                    .fullscreen = false,
                    .resizable = true,
                    .maximized = false,
                    .minimized = false,
                },
                "Terminal Emulator");
            create_pending = true;
        }

        desktop_event_t event;
        int poll_result = desktop_poll_event(&event);
        if (poll_result != 0) {
            _pump_shell_commands(&terminal);
            _present_if_dirty(framebuffer_ready, window_id, &gfx_context, &terminal);
            if (_wait_for_terminal_activity(&terminal, desktop_handle)) {
                if (created)
                    desktop_destroy_window((uint16_t) window_id);
                break;
            }
            continue;
        }

        _pump_shell_commands(&terminal);

        if (event.type == DESKTOP_EVENT_WINDOW_CREATED && create_pending
            && event.sequence == create_sequence) {
            if (event.data.created.status != WINDOW_CREATED_SUCCESS) {
                create_pending = false;
                continue;
            }

            created = true;
            create_pending = false;
            framebuffer_ready = false;
            window_id = event.data.created.window_id;
            next_dimensions_event = TERM_EVENT_WINDOW_INFO;
            if (desktop_request_window_framebuffer(
                    (uint16_t) window_id, event.data.created.width, event.data.created.height)
                == 1) {
                _resize_terminal(
                    &terminal,
                    &gfx_context,
                    window_id,
                    next_dimensions_event,
                    &framebuffer_ready);
            }
            continue;
        }

        if (event.type == DESKTOP_EVENT_FRAMEBUFFER_READY
            && event.data.framebuffer_ready.id == window_id
            && desktop_handle_framebuffer_event(&event, &gfx_context) == 1) {
            gfx_set_target_fps(&gfx_context, 30);
            _resize_terminal(
                &terminal, &gfx_context, window_id, next_dimensions_event, &framebuffer_ready);
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_RESIZED
            && event.data.resized.window_id == window_id) {
            next_dimensions_event = TERM_EVENT_WINDOW_RESIZED;
            framebuffer_ready = false;
            if (desktop_request_window_framebuffer(
                    (uint16_t) window_id,
                    event.data.resized.new_width,
                    event.data.resized.new_height)
                == 1) {
                _resize_terminal(
                    &terminal,
                    &gfx_context,
                    window_id,
                    next_dimensions_event,
                    &framebuffer_ready);
            }
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_KEYBOARD
            && event.data.keyboard.window_id == window_id) {
            _handle_keyboard_event(&terminal, &event.data.keyboard);
            _pump_shell_commands(&terminal);
            _present_if_dirty(framebuffer_ready, window_id, &gfx_context, &terminal);
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_CLOSE && event.data.close.window_id == window_id) {
            desktop_destroy_window((uint16_t) window_id);
            framebuffer_ready = false;
            continue;
        }

        if (event.type == DESKTOP_EVENT_WINDOW_CLOSED && event.data.closed.window_id == window_id)
            break;
    }

    _term_deinit(&terminal);
    return 0;
}
