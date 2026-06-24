/*
 * Copyright (c) 2025, Ibrahim KAIKAA
 * SPDX-License-Identifier: GPL-3.0
 */

#define DOOM_IMPLEMENT_MALLOC
#define DOOM_IMPLEMENTATION
#include <PureDOOM.h>

#include <debug.h>
#include <libdesktop.h>
#include <libgfx.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    int fd;
    uint64_t size;
    bool writable;
    bool size_known;
} doom_file_t;

static int doom_tell_fd(int fd)
{
    return rsmgr_seek(fd, 0, SEEK_CUR);
}

static int doom_ensure_size(doom_file_t *file)
{
    if (!file)
        return -1;

    if (file->size_known)
        return 0;

    int original_pos = rsmgr_seek(file->fd, 0, SEEK_CUR);
    if (original_pos < 0)
        return -1;

    int end_pos = rsmgr_seek(file->fd, 0, SEEK_END);
    if (end_pos < 0)
        return -1;

    file->size = (uint64_t) end_pos;
    file->size_known = true;

    return rsmgr_seek(file->fd, original_pos, SEEK_SET) < 0 ? -1 : 0;
}

static void doom_print_override(const char *str)
{
    debug_log("%s", str);
}

static void *doom_malloc_override(int size)
{
    return malloc((size_t) size);
}

static void doom_free_override(void *ptr)
{
    free(ptr);
}

static bool mode_has_plus(const char *mode)
{
    if (!mode)
        return false;
    for (const char *p = mode; *p; ++p) {
        if (*p == '+')
            return true;
    }
    return false;
}

static int doom_create_file(const char *filename)
{
    if (!filename)
        return -1;

    return rsmgr_create(filename, RSRC_TYPE_RESOURCE);
}

static void *doom_open_override(const char *filename, const char *mode)
{
    if (!filename || !mode)
        return NULL;

    bool writable = false;

    if (mode[0] == 'r') {
        writable = false;
        if (mode_has_plus(mode)) {
            writable = true;
        }
    } else if (mode[0] == 'w') {
        writable = true;
        if (mode_has_plus(mode)) {
            writable = true;
        }
    } else if (mode[0] == 'a') {
        writable = true;
        if (mode_has_plus(mode)) {
            writable = true;
        }
    }

    int fd = rsmgr_open(filename);
    if (fd < 0 && (mode[0] == 'w' || mode[0] == 'a'))
        fd = doom_create_file(filename);
    if (fd < 0)
        return NULL;

    doom_file_t *file = (doom_file_t *) malloc(sizeof(doom_file_t));
    if (!file) {
        rsmgr_close(fd);
        return NULL;
    }

    memset(file, 0, sizeof(*file));
    file->fd = fd;
    file->writable = writable;
    file->size = 0;
    file->size_known = false;

    if (mode[0] == 'a') {
        if (doom_ensure_size(file) < 0) {
            rsmgr_close(fd);
            free(file);
            return NULL;
        }
        if (rsmgr_seek(fd, (int) file->size, SEEK_SET) < 0) {
            rsmgr_close(fd);
            free(file);
            return NULL;
        }
    }

    return file;
}

static void doom_close_override(void *handle)
{
    if (!handle)
        return;

    doom_file_t *file = (doom_file_t *) handle;
    rsmgr_close(file->fd);
    free(file);
}

static int doom_read_override(void *handle, void *buf, int count)
{
    if (!handle || !buf || count <= 0)
        return 0;

    doom_file_t *file = (doom_file_t *) handle;
    int read_bytes = rsmgr_read(file->fd, buf, (uint32_t) count);
    if (read_bytes == 0 && !file->size_known) {
        int pos = doom_tell_fd(file->fd);
        if (pos >= 0)
            file->size = (uint64_t) pos;
        file->size_known = true;
    }
    return read_bytes;
}

static int doom_write_override(void *handle, const void *buf, int count)
{
    if (!handle || !buf || count <= 0)
        return 0;

    doom_file_t *file = (doom_file_t *) handle;
    if (!file->writable)
        return 0;

    int written = rsmgr_write(file->fd, buf, (uint32_t) count);
    if (written > 0) {
        file->size_known = false;
    }
    return written;
}

static int doom_seek_override(void *handle, int offset, doom_seek_t origin)
{
    if (!handle)
        return -1;

    doom_file_t *file = (doom_file_t *) handle;
    int base = 0;

    switch (origin) {
    case DOOM_SEEK_CUR:
        base = doom_tell_fd(file->fd);
        break;
    case DOOM_SEEK_END:
        if (doom_ensure_size(file) < 0)
            return -1;
        base = (int) file->size;
        break;
    case DOOM_SEEK_SET:
    default:
        base = 0;
        break;
    }

    if (base < 0)
        return -1;

    int64_t target = (int64_t) base + (int64_t) offset;
    if (target < 0)
        target = 0;
    if (target > INT32_MAX)
        target = INT32_MAX;

    return rsmgr_seek(file->fd, (int) target, SEEK_SET) < 0 ? -1 : 0;
}

static int doom_tell_override(void *handle)
{
    if (!handle)
        return -1;

    doom_file_t *file = (doom_file_t *) handle;
    return doom_tell_fd(file->fd);
}

static int doom_eof_override(void *handle)
{
    if (!handle)
        return 1;

    doom_file_t *file = (doom_file_t *) handle;
    if (!file->size_known)
        return doom_ensure_size(file) < 0 ? 1 : 0;

    int pos = doom_tell_fd(file->fd);
    if (pos < 0)
        return 1;
    return (uint64_t) pos >= file->size;
}

static void doom_gettime_override(int *sec, int *usec)
{
    uint64_t ticks = get_ticks();
    if (sec)
        *sec = (int) (ticks / 1000);
    if (usec)
        *usec = (int) ((ticks % 1000) * 1000);
}

static void doom_exit_override(int code)
{
    exit(code);
}

static char *doom_getenv_override(const char *var)
{
    if (!var)
        return NULL;
    if (strcmp(var, "DOOMWADDIR") == 0)
        return "file:/system/assets";
    if (strcmp(var, "HOME") == 0)
        return "file:/system/";
    return NULL;
}

static doom_key_t doom_key_from_scancode(uint8_t scancode)
{
    switch (scancode) {
    case 0x01:
        return DOOM_KEY_ESCAPE;
    case 0x0F:
        return DOOM_KEY_TAB;
    case 0x1C:
        return DOOM_KEY_ENTER;
    case 0x39:
        return DOOM_KEY_SPACE;
    case 0x0E:
        return DOOM_KEY_BACKSPACE;

    case 0x02:
        return DOOM_KEY_1;
    case 0x03:
        return DOOM_KEY_2;
    case 0x04:
        return DOOM_KEY_3;
    case 0x05:
        return DOOM_KEY_4;
    case 0x06:
        return DOOM_KEY_5;
    case 0x07:
        return DOOM_KEY_6;
    case 0x08:
        return DOOM_KEY_7;
    case 0x09:
        return DOOM_KEY_8;
    case 0x0A:
        return DOOM_KEY_9;
    case 0x0B:
        return DOOM_KEY_0;

    case 0x10:
        return DOOM_KEY_Q;
    case 0x11:
        return DOOM_KEY_W;
    case 0x12:
        return DOOM_KEY_E;
    case 0x13:
        return DOOM_KEY_R;
    case 0x14:
        return DOOM_KEY_T;
    case 0x15:
        return DOOM_KEY_Y;
    case 0x16:
        return DOOM_KEY_U;
    case 0x17:
        return DOOM_KEY_I;
    case 0x18:
        return DOOM_KEY_O;
    case 0x19:
        return DOOM_KEY_P;

    case 0x1E:
        return DOOM_KEY_A;
    case 0x1F:
        return DOOM_KEY_S;
    case 0x20:
        return DOOM_KEY_D;
    case 0x21:
        return DOOM_KEY_F;
    case 0x22:
        return DOOM_KEY_G;
    case 0x23:
        return DOOM_KEY_H;
    case 0x24:
        return DOOM_KEY_J;
    case 0x25:
        return DOOM_KEY_K;
    case 0x26:
        return DOOM_KEY_L;

    case 0x2C:
        return DOOM_KEY_Z;
    case 0x2D:
        return DOOM_KEY_X;
    case 0x2E:
        return DOOM_KEY_C;
    case 0x2F:
        return DOOM_KEY_V;
    case 0x30:
        return DOOM_KEY_B;
    case 0x31:
        return DOOM_KEY_N;
    case 0x32:
        return DOOM_KEY_M;

    case 0x33:
        return DOOM_KEY_COMMA;
    case 0x34:
        return DOOM_KEY_PERIOD;
    case 0x35:
        return DOOM_KEY_SLASH;
    case 0x27:
        return DOOM_KEY_SEMICOLON;
    case 0x0C:
        return DOOM_KEY_MINUS;
    case 0x0D:
        return DOOM_KEY_EQUALS;
    case 0x1A:
        return DOOM_KEY_LEFT_BRACKET;
    case 0x1B:
        return DOOM_KEY_RIGHT_BRACKET;
    case 0x28:
        return DOOM_KEY_APOSTROPHE;

    case 0x1D:
        return DOOM_KEY_CTRL;
    case 0x2A:
    case 0x36:
        return DOOM_KEY_SHIFT;
    case 0x38:
        return DOOM_KEY_ALT;

    case 0x3B:
        return DOOM_KEY_F1;
    case 0x3C:
        return DOOM_KEY_F2;
    case 0x3D:
        return DOOM_KEY_F3;
    case 0x3E:
        return DOOM_KEY_F4;
    case 0x3F:
        return DOOM_KEY_F5;
    case 0x40:
        return DOOM_KEY_F6;
    case 0x41:
        return DOOM_KEY_F7;
    case 0x42:
        return DOOM_KEY_F8;
    case 0x43:
        return DOOM_KEY_F9;
    case 0x44:
        return DOOM_KEY_F10;
    case 0x57:
        return DOOM_KEY_F11;
    case 0x58:
        return DOOM_KEY_F12;

    case 0x48:
        return DOOM_KEY_UP_ARROW;
    case 0x50:
        return DOOM_KEY_DOWN_ARROW;
    case 0x4B:
        return DOOM_KEY_LEFT_ARROW;
    case 0x4D:
        return DOOM_KEY_RIGHT_ARROW;

    default:
        return DOOM_KEY_UNKNOWN;
    }
}

enum {
    KEYBOARD_HOLD = 0,
    KEYBOARD_PRESSED = 1,
    KEYBOARD_RELEASED = 2,
};

static void handle_doom_keyboard_event(const window_keyboard_event_t *event)
{
    if (!event)
        return;

    doom_key_t key = doom_key_from_scancode(event->keyboard.scancode);
    if (key == DOOM_KEY_UNKNOWN)
        return;

    if (event->keyboard.action == KEYBOARD_RELEASED)
        doom_key_up(key);
    else if (event->keyboard.action == KEYBOARD_PRESSED || event->keyboard.action == KEYBOARD_HOLD)
        doom_key_down(key);
}

static void blit_doom_frame(gfx_context_t *ctx, uint32_t win_w, uint32_t win_h)
{
    if (!ctx || !ctx->backbuffer)
        return;

    const uint32_t src_w = 320;
    const uint32_t src_h = 200;
    const unsigned char *src = doom_get_framebuffer(4);
    if (!src)
        return;

    uint32_t dst_w = win_w;
    uint32_t dst_h = win_h;
    if (dst_w == 0 || dst_h == 0)
        return;

    uint32_t *dst = (uint32_t *) ctx->backbuffer;
    uint32_t dst_pitch = (uint32_t) ctx->width;

    for (uint32_t y = 0; y < dst_h; y++) {
        uint32_t src_y = (uint32_t) ((uint64_t) y * src_h / dst_h);
        const unsigned char *src_row = src + src_y * src_w * 4;
        uint32_t *dst_row = dst + y * dst_pitch;

        for (uint32_t x = 0; x < dst_w; x++) {
            uint32_t src_x = (uint32_t) ((uint64_t) x * src_w / dst_w);
            const unsigned char *pixel = src_row + src_x * 4;
            uint8_t r = pixel[0];
            uint8_t g = pixel[1];
            uint8_t b = pixel[2];
            uint8_t a = 0xFF;
            dst_row[x] = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8)
                         | (uint32_t) b;
        }
    }
}

int main(int argc, char *argv[])
{
    doom_set_print(doom_print_override);
    doom_set_malloc(doom_malloc_override, doom_free_override);
    doom_set_file_io(
        doom_open_override,
        doom_close_override,
        doom_read_override,
        doom_write_override,
        doom_seek_override,
        doom_tell_override,
        doom_eof_override);
    doom_set_gettime(doom_gettime_override);
    doom_set_exit(doom_exit_override);
    doom_set_getenv(doom_getenv_override);

    if (argc < 2 || argv[1] == NULL) {
        debug_log("doom: missing WAD file argument\n");
        return 1;
    }

    doom_init(
        argc,
        argv,
        DOOM_FLAG_MENU_DARKEN_BG | DOOM_FLAG_HIDE_MUSIC_OPTIONS | DOOM_FLAG_HIDE_SOUND_OPTIONS);

    bool created = false;
    bool create_pending = false;
    bool framebuffer_ready = false;

    uint32_t create_sequence = 0;
    uint32_t window_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    gfx_context_t framebuffer = {0};

    if (desktop_connect() != 0) {
        debug_log("doom: failed to connect to desktop\n");
        return 1;
    }

    while (1) {
        if (!created && !create_pending) {
            create_sequence = desktop_create_window(640, 480, (window_flags_t){0}, "DOOM");
            create_pending = true;
        }

        desktop_event_t event;
        int poll_result = desktop_poll_event(&event);

        if (poll_result == 0) {
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
                width = event.data.created.width;
                height = event.data.created.height;
                desktop_request_window_framebuffer(window_id, width, height);
                continue;
            }

            if (event.type == DESKTOP_EVENT_FRAMEBUFFER_READY
                && event.data.framebuffer_ready.id == window_id
                && desktop_handle_framebuffer_event(&event, &framebuffer) == 1) {
                gfx_set_target_fps(&framebuffer, 60);
                framebuffer_ready = true;
                continue;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_KEYBOARD
                && event.data.keyboard.window_id == window_id) {
                handle_doom_keyboard_event(&event.data.keyboard);
                continue;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_CLOSE
                && event.data.close.window_id == window_id) {
                desktop_destroy_window((uint16_t) window_id);
                framebuffer_ready = false;
                continue;
            }

            if (event.type == DESKTOP_EVENT_WINDOW_CLOSED
                && event.data.closed.window_id == window_id) {
                break;
            }
        }

        if (created && framebuffer_ready) {
            doom_update();

            gfx_begin_frame(&framebuffer);
            blit_doom_frame(&framebuffer, width, height);
            gfx_end_frame(&framebuffer);

            desktop_present_window((uint16_t) window_id);
        } else {
            usleep(1);
        }
    }

    return 0;
}
