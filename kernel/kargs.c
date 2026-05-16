/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/kargs.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>

static bool _cmdline_is_separator(char c)
{
    return c == ' ' || c == '\t';
}

static char *_cmdline_copy_slice(const char *start, size_t len)
{
    char *copy = kmalloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static cmdline_arg_t *_cmdline_new_arg(
    const char *key, size_t key_len, const char *value, size_t value_len)
{
    if (key_len == 0)
        return NULL;

    cmdline_arg_t *arg = kmalloc(sizeof(cmdline_arg_t));
    if (arg == NULL)
        return NULL;

    arg->key = _cmdline_copy_slice(key, key_len);
    arg->value = _cmdline_copy_slice(value, value_len);
    arg->next = NULL;

    if (arg->key == NULL || arg->value == NULL) {
        kfree(arg->key);
        kfree(arg->value);
        kfree(arg);
        return NULL;
    }

    return arg;
}

cmdline_arg_t *load_kernel_args(struct limine_executable_cmdline_response *response)
{
    if (response == NULL || response->cmdline == NULL)
        return NULL;

    cmdline_arg_t *head = NULL;
    cmdline_arg_t *tail = NULL;
    const char *cmdline = response->cmdline;

    while (*cmdline != '\0') {
        while (_cmdline_is_separator(*cmdline))
            cmdline++;

        if (*cmdline == '\0')
            break;

        const char *key = cmdline;
        while (*cmdline != '\0' && !_cmdline_is_separator(*cmdline) && *cmdline != '=')
            cmdline++;
        size_t key_len = cmdline - key;

        const char *value = "";
        size_t value_len = 0;
        if (*cmdline == '=') {
            cmdline++;
            value = cmdline;
            while (*cmdline != '\0' && !_cmdline_is_separator(*cmdline))
                cmdline++;
            value_len = cmdline - value;
        }

        cmdline_arg_t *arg = _cmdline_new_arg(key, key_len, value, value_len);
        if (arg == NULL)
            continue;

        if (tail != NULL)
            tail->next = arg;
        else
            head = arg;
        tail = arg;
    }

    return head;
}

char *get_kernel_arg(cmdline_arg_t *args, const char *key)
{
    for (cmdline_arg_t *arg = args; arg != NULL; arg = arg->next)
        if (strcmp(arg->key, key) == 0)
            return arg->value;
    return NULL;
}
