/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

#define SHELL_INPUT_LEN 4096
#define SHELL_MAX_ARGS 32
#define SHELL_COPY_BUFFER_LEN 1024

static char _cwd[RSRC_PATH_MAX_LEN] = "file:/system/";

static void _write_tin_bytes(const void *bytes, uint32_t length)
{
    if (bytes == NULL || length == 0)
        return;

    term_write_command(TERM_RD_TIN, TERM_COMMAND_WRITE_VT100, bytes, length);
}

static void _write_tin(const char *text)
{
    if (text == NULL)
        return;
    _write_tin_bytes(text, (uint32_t) strlen(text));
}

static void _write_prompt(void)
{
    _write_tin("\x1b[32m");
    _write_tin(_cwd);
    _write_tin("> \x1b[39m");
}

static void _write_line(const char *text)
{
    _write_tin(text);
    _write_tin("\r\n");
}

static void _write_uint64(uint64_t value)
{
    char buffer[32];
    int pos = sizeof(buffer) - 1;
    buffer[pos] = '\0';

    if (value == 0) {
        _write_tin("0");
        return;
    }

    while (value != 0 && pos > 0) {
        buffer[--pos] = (char) ('0' + (value % 10));
        value /= 10;
    }

    _write_tin(&buffer[pos]);
}

static void _write_field_uint64(const char *name, uint64_t value)
{
    _write_tin(name);
    _write_tin(": ");
    _write_uint64(value);
    _write_tin("\r\n");
}

static void _write_field_text(const char *name, const char *value)
{
    _write_tin(name);
    _write_tin(": ");
    _write_line(value != NULL ? value : "");
}

static bool _path_has_domain(const char *path)
{
    if (path == NULL)
        return false;

    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == ':')
            return i > 0 && path[i + 1] == '/';
    }
    return false;
}

static const char *_last_path_component(const char *path)
{
    const char *last = path;

    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/')
            last = p + 1;
    }

    return last;
}

static int _parse_args(char *input, char **argv, int max_args)
{
    int argc = 0;
    char *p = input;

    while (*p != '\0' && argc < max_args) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
            p++;
        if (*p == '\0')
            break;

        *p = '\0';
        p++;
    }

    return argc;
}

static void _append_component(
    char *path, uint32_t *length, const char *component, uint32_t component_len)
{
    if (component_len == 0 || (component_len == 1 && component[0] == '.'))
        return;

    if (component_len == 2 && component[0] == '.' && component[1] == '.') {
        if (*length <= 0)
            return;
        if (*length > 0 && path[*length - 1] == '/')
            (*length)--;
        while (*length > 0 && path[*length - 1] != '/')
            (*length)--;
        path[*length] = '\0';
        return;
    }

    if (*length == 0 || path[*length - 1] != '/') {
        if (*length + 1 >= RSRC_PATH_MAX_LEN)
            return;
        path[(*length)++] = '/';
    }

    if (*length + component_len >= RSRC_PATH_MAX_LEN)
        component_len = RSRC_PATH_MAX_LEN - *length - 1;

    memcpy(path + *length, component, component_len);
    *length += component_len;
    path[*length] = '\0';
}

static bool _normalize_path(const char *path, char *out, bool trailing_slash)
{
    if (path == NULL || out == NULL || *path == '\0')
        return false;

    char prefix[RSRC_NAME_MAX_LEN];
    char body[RSRC_PATH_MAX_LEN];
    const char *body_start = path;

    if (_path_has_domain(path)) {
        uint32_t prefix_len = 0;
        while (path[prefix_len] != ':' && path[prefix_len] != '\0')
            prefix_len++;
        if (prefix_len == 0 || prefix_len + 1 >= sizeof(prefix))
            return false;
        memcpy(prefix, path, prefix_len + 1);
        prefix[prefix_len + 1] = '\0';
        body_start = path + prefix_len + 1;
    } else {
        uint32_t prefix_len = 0;
        while (_cwd[prefix_len] != ':' && _cwd[prefix_len] != '\0')
            prefix_len++;
        if (_cwd[prefix_len] != ':' || prefix_len + 1 >= sizeof(prefix))
            return false;
        memcpy(prefix, _cwd, prefix_len + 1);
        prefix[prefix_len + 1] = '\0';

        if (path[0] == '/') {
            body_start = path;
        } else {
            uint32_t current_body_len = 0;
            const char *current_body = _cwd + prefix_len + 1;
            while (current_body[current_body_len] != '\0' && current_body_len < sizeof(body) - 1) {
                body[current_body_len] = current_body[current_body_len];
                current_body_len++;
            }
            body[current_body_len] = '\0';
        }
    }

    if (_path_has_domain(path) || path[0] == '/') {
        body[0] = '/';
        body[1] = '\0';
    }

    uint32_t body_len = (uint32_t) strlen(body);
    const char *p = body_start;
    while (*p == '/')
        p++;

    while (*p != '\0') {
        const char *start = p;
        while (*p != '\0' && *p != '/')
            p++;
        _append_component(body, &body_len, start, (uint32_t) (p - start));
        while (*p == '/')
            p++;
    }

    if (body_len == 0) {
        body[0] = '/';
        body[1] = '\0';
        body_len = 1;
    }

    if (trailing_slash && body[body_len - 1] != '/') {
        if (body_len + 1 >= sizeof(body))
            return false;
        body[body_len++] = '/';
        body[body_len] = '\0';
    }

    uint32_t prefix_len = (uint32_t) strlen(prefix);
    if (prefix_len + body_len + 1 > RSRC_PATH_MAX_LEN)
        return false;

    memcpy(out, prefix, prefix_len);
    memcpy(out + prefix_len, body, body_len + 1);
    return true;
}

static bool _split_parent_name(const char *path, char *parent, char *name)
{
    char normalized[RSRC_PATH_MAX_LEN];
    if (!_normalize_path(path, normalized, false))
        return false;

    uint32_t len = (uint32_t) strlen(normalized);
    while (len > 0 && normalized[len - 1] == '/' && !(len >= 2 && normalized[len - 2] == ':')) {
        normalized[--len] = '\0';
    }

    const char *last = _last_path_component(normalized);
    if (*last == '\0')
        return false;

    strcpy(name, last);
    uint32_t parent_len = (uint32_t) (last - normalized);
    if (parent_len == 0)
        return false;
    memcpy(parent, normalized, parent_len);
    parent[parent_len] = '\0';
    if (parent[parent_len - 1] == '/' && parent_len >= 2 && parent[parent_len - 2] != ':')
        parent[parent_len - 1] = '\0';
    return true;
}

static int _remove_path(const char *path)
{
    char parent_path[RSRC_PATH_MAX_LEN];
    char name[RSRC_NAME_MAX_LEN];
    if (!_split_parent_name(path, parent_path, name))
        return -1;

    int parent = open(parent_path);
    if (parent < 0)
        return -1;

    int result = (int) syscall2(SYSCALL_RSRC_REMOVE, parent, (long) name);
    close(parent);
    return result;
}

static void _cmd_exit(void)
{
    exit(0);
}

static void _cmd_help(void)
{
    _write_line("built-ins:");
    _write_line("  help");
    _write_line("  chdir <path>");
    _write_line("  list [path]");
    _write_line("  info <path>");
    _write_line("  copy <source> <destination>");
    _write_line("  move <source> <destination>");
    _write_line("  mkdir <path>");
    _write_line("  remove <path>");
    _write_line("  create <path>");
    _write_line("  exec <path> [args...]");
    _write_line("  exit");
}

static const char *_domain_name(rsrc_domain_id_t domain_id)
{
    switch (domain_id) {
    case RSRC_DOMAIN_NULL:
        return "null";
    case RSRC_DOMAIN_FILE:
        return "file";
    case RSRC_DOMAIN_DEVICE:
        return "device";
    case RSRC_DOMAIN_CHANNEL:
        return "channel";
    case RSRC_DOMAIN_SHM:
        return "shm";
    case RSRC_DOMAIN_TASK:
        return "task";
    case RSRC_DOMAIN_PIPE:
        return "pipe";
    default:
        return "unknown";
    }
}

static const char *_type_name(rsrc_type_t type)
{
    switch (type) {
    case RSRC_TYPE_NULL:
        return "null";
    case RSRC_TYPE_COLLECTION:
        return "collection";
    case RSRC_TYPE_RESOURCE:
        return "resource";
    default:
        return "unknown";
    }
}

static const char *_task_state_name(rsrc_task_state_t state)
{
    switch (state) {
    case RSRC_TASK_STATE_RUNNABLE:
        return "runnable";
    case RSRC_TASK_STATE_SLEEPING:
        return "sleeping";
    case RSRC_TASK_STATE_EXITING:
        return "exiting";
    default:
        return "unknown";
    }
}

static void _cmd_chdir(int argc, char **argv)
{
    if (argc != 2) {
        _write_line("usage: chdir <path>");
        return;
    }

    char normalized[RSRC_PATH_MAX_LEN];
    if (!_normalize_path(argv[1], normalized, true) || chcwd(argv[1]) < 0) {
        _write_line("chdir: failed");
        return;
    }

    strcpy(_cwd, normalized);
}

static void _cmd_list(int argc, char **argv)
{
    const char *path = argc >= 2 ? argv[1] : ".";
    char dir_path[RSRC_PATH_MAX_LEN];
    if (!_normalize_path(path, dir_path, true)) {
        _write_line("list: invalid path");
        return;
    }

    int fd = open(path);
    if (fd < 0) {
        _write_line("list: failed to open path");
        return;
    }

    char buffer[TERM_MAX_PAYLOAD];
    int bytes = getdents(fd, buffer, sizeof(buffer));
    close(fd);
    if (bytes < 0) {
        _write_line("list: failed");
        return;
    }

    int offset = 0;
    while (offset + (int) sizeof(uint32_t) <= bytes) {
        uint32_t record_size = 0;
        memcpy(&record_size, buffer + offset, sizeof(record_size));
        if (record_size <= sizeof(uint32_t) || offset + (int) record_size > bytes)
            break;

        const char *name = buffer + offset + sizeof(uint32_t);
        bool is_directory = false;
        char child_path[RSRC_PATH_MAX_LEN];
        size_t dir_len = strlen(dir_path);
        size_t name_len = strlen(name);
        if (dir_len + name_len + 1 <= sizeof(child_path)) {
            memcpy(child_path, dir_path, dir_len);
            memcpy(child_path + dir_len, name, name_len + 1);

            int child = open(child_path);
            if (child >= 0) {
                rsrc_info_t info = {0};
                if (rsmgr_describe(child, &info) == 0 && info.type == RSRC_TYPE_COLLECTION)
                    is_directory = true;
                close(child);
            }
        }

        _write_tin(name);
        if (is_directory)
            _write_tin("/");
        _write_tin("\r\n");
        offset += (int) record_size;
    }
}

static void _cmd_info(int argc, char **argv)
{
    if (argc != 2) {
        _write_line("usage: info <path>");
        return;
    }

    char normalized[RSRC_PATH_MAX_LEN];
    bool has_normalized_path = _normalize_path(argv[1], normalized, false);

    int fd = open(argv[1]);
    if (fd < 0) {
        _write_line("info: failed to open path");
        return;
    }

    rsrc_info_t info = {0};
    if (rsmgr_describe(fd, &info) != 0) {
        close(fd);
        _write_line("info: failed");
        return;
    }

    close(fd);

    _write_field_text("path", has_normalized_path ? normalized : argv[1]);
    _write_field_text("domain", _domain_name(info.domain_id));
    _write_field_text("type", _type_name(info.type));
    _write_field_uint64("id", info.id);

    switch (info.domain_id) {
    case RSRC_DOMAIN_FILE:
        _write_field_uint64("size", info.file.size);
        _write_field_uint64("child_count", info.file.child_count);
        break;
    case RSRC_DOMAIN_CHANNEL:
        _write_field_uint64("channel_id", info.channel.channel_id);
        _write_field_uint64("owner_task_id", info.channel.owner_task_id);
        break;
    case RSRC_DOMAIN_SHM:
        _write_field_uint64("size", info.shm.size);
        _write_field_uint64("page_count", info.shm.page_count);
        break;
    case RSRC_DOMAIN_TASK:
        _write_field_uint64("task_id", info.task.task_id);
        _write_field_uint64("parent_task_id", info.task.parent_task_id);
        _write_field_uint64("child_count", info.task.child_count);
        _write_field_text("state", _task_state_name(info.task.state));
        if (info.task.name[0] != '\0')
            _write_field_text("name", info.task.name);
        if (info.task.path[0] != '\0')
            _write_field_text("executable", info.task.path);
        break;
    default:
        break;
    }
}

static int _copy_file(const char *src_path, const char *dst_path)
{
    int src = open(src_path);
    if (src < 0)
        return -1;

    int existing_dst = open(dst_path);
    if (existing_dst >= 0) {
        close(existing_dst);
        if (_remove_path(dst_path) < 0) {
            close(src);
            return -1;
        }
    }

    int dst = file_create(dst_path);
    if (dst < 0) {
        close(src);
        return -1;
    }

    char buffer[SHELL_COPY_BUFFER_LEN];
    while (1) {
        int bytes = read(src, buffer, sizeof(buffer));
        if (bytes < 0) {
            close(src);
            close(dst);
            return -1;
        }
        if (bytes == 0)
            break;
        if (write(dst, buffer, (uint32_t) bytes) < 0) {
            close(src);
            close(dst);
            return -1;
        }
    }

    close(src);
    close(dst);
    return 0;
}

static void _cmd_copy(int argc, char **argv)
{
    if (argc != 3) {
        _write_line("usage: copy <source> <destination>");
        return;
    }

    if (_copy_file(argv[1], argv[2]) < 0)
        _write_line("copy: failed");
}

static void _cmd_move(int argc, char **argv)
{
    if (argc != 3) {
        _write_line("usage: move <source> <destination>");
        return;
    }

    if (_copy_file(argv[1], argv[2]) < 0 || _remove_path(argv[1]) < 0)
        _write_line("move: failed");
}

static void _cmd_mkdir(int argc, char **argv)
{
    if (argc != 2) {
        _write_line("usage: mkdir <path>");
        return;
    }

    if (mkdir(argv[1], 0) < 0)
        _write_line("mkdir: failed");
}

static void _cmd_remove(int argc, char **argv)
{
    if (argc != 2) {
        _write_line("usage: remove <path>");
        return;
    }

    if (_remove_path(argv[1]) < 0)
        _write_line("remove: failed");
}

static void _cmd_create(int argc, char **argv)
{
    if (argc != 2) {
        _write_line("usage: create <path>");
        return;
    }

    int fd = file_create(argv[1]);
    if (fd < 0) {
        _write_line("create: failed");
        return;
    }
    close(fd);
}

static void _cmd_exec(int argc, char **argv)
{
    if (argc < 2) {
        _write_line("usage: exec <path> [args...]");
        return;
    }

    const char *exec_argv[SHELL_MAX_ARGS];
    for (int i = 1; i < argc; i++)
        exec_argv[i - 1] = argv[i];

    const task_create_inherit_t inherit[] = {
        {.current_descriptor = TERM_RD_TIN, .target_descriptor = TERM_RD_TIN},
        {.current_descriptor = TERM_RD_TOUT, .target_descriptor = TERM_RD_TOUT},
    };
    int task
        = task_create(argc - 1, exec_argv, inherit, (int) (sizeof(inherit) / sizeof(inherit[0])));
    if (task < 0) {
        _write_line("exec: failed");
        return;
    }

    rsrc_poll_t poll = {.handle = task, .events = RSRC_POLL_READ};
    rsmgr_poll(&poll, 1);
    close(task);
}

static void _run_command(char *input)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = _parse_args(input, argv, SHELL_MAX_ARGS);
    if (argc == 0)
        return;

    if (strcmp(argv[0], "help") == 0)
        _cmd_help();
    else if (strcmp(argv[0], "chdir") == 0)
        _cmd_chdir(argc, argv);
    else if (strcmp(argv[0], "list") == 0)
        _cmd_list(argc, argv);
    else if (strcmp(argv[0], "info") == 0)
        _cmd_info(argc, argv);
    else if (strcmp(argv[0], "copy") == 0)
        _cmd_copy(argc, argv);
    else if (strcmp(argv[0], "move") == 0)
        _cmd_move(argc, argv);
    else if (strcmp(argv[0], "mkdir") == 0)
        _cmd_mkdir(argc, argv);
    else if (strcmp(argv[0], "remove") == 0)
        _cmd_remove(argc, argv);
    else if (strcmp(argv[0], "create") == 0)
        _cmd_create(argc, argv);
    else if (strcmp(argv[0], "exec") == 0)
        _cmd_exec(argc, argv);
    else if (strcmp(argv[0], "exit") == 0)
        _cmd_exit();
    else
        _write_line("unknown command");
}

static void _skip_vt100_sequence(const char *payload, uint32_t length, uint32_t *index)
{
    if (*index + 1 >= length || payload[*index] != '\x1b')
        return;

    (*index)++;
    if (payload[*index] == '[') {
        while (*index + 1 < length) {
            (*index)++;
            if (payload[*index] >= '@' && payload[*index] <= '~')
                break;
        }
    }
}

int main(void)
{
    uint32_t input_len = 0;
    char input[SHELL_INPUT_LEN];

    chcwd(_cwd);

    _write_tin("MONOLITH shell\r\n");
    _write_tin("Type 'help' for available commands.\r\n\r\n");
    _write_prompt();

    while (1) {
        char payload[TERM_MAX_PAYLOAD];
        term_event_t event = {0};
        rsrc_poll_t poll = {.handle = TERM_RD_TOUT, .events = RSRC_POLL_READ};
        rsmgr_poll(&poll, 1);
        if (term_read_event(TERM_RD_TOUT, &event, payload, sizeof(payload)) != 1)
            continue;
        if (event.type != TERM_EVENT_INPUT_VT100)
            continue;

        for (uint32_t i = 0; i < event.length; i++) {
            char ch = payload[i];
            if (ch == '\x1b') {
                _skip_vt100_sequence(payload, event.length, &i);
                continue;
            }

            if (ch == '\b' || ch == 0x7F) {
                if (input_len > 0) {
                    input_len--;
                    _write_tin("\b \b");
                }
                continue;
            }

            if (ch == '\r' || ch == '\n') {
                _write_tin("\r\n");
                input[input_len] = '\0';
                _run_command(input);
                input_len = 0;
                _write_prompt();
                continue;
            }

            if ((unsigned char) ch < 0x20 || input_len >= SHELL_INPUT_LEN - 1)
                continue;

            input[input_len++] = ch;
            _write_tin_bytes(&ch, 1);
        }
    }

    return 0;
}
