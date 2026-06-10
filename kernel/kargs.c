/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/kargs.h>
#include <kernel/klibc/string.h>

static const char *_kernel_cmdline = NULL;

static bool _is_arg_separator(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void load_kernel_args(const char *cmdline)
{
    _kernel_cmdline = cmdline;
}

bool get_kernel_arg(const char *key, char *out_value, size_t out_value_len)
{
    if (out_value == NULL || out_value_len == 0)
        return false;

    out_value[0] = '\0';

    if (_kernel_cmdline == NULL || key == NULL || key[0] == '\0')
        return false;

    size_t key_len = strlen(key);
    const char *arg = _kernel_cmdline;

    while (*arg != '\0') {
        while (_is_arg_separator(*arg))
            arg++;

        if (*arg == '\0')
            break;

        const char *arg_end = arg;
        while (*arg_end != '\0' && !_is_arg_separator(*arg_end))
            arg_end++;

        const char *equals = arg;
        while (equals < arg_end && *equals != '=')
            equals++;

        if (equals < arg_end && (size_t) (equals - arg) == key_len
            && strncmp(arg, key, key_len) == 0) {
            const char *value = equals + 1;
            size_t value_len = (size_t) (arg_end - value);
            size_t copy_len = value_len;

            if (copy_len >= out_value_len)
                copy_len = out_value_len - 1;

            for (size_t i = 0; i < copy_len; i++)
                out_value[i] = value[i];
            out_value[copy_len] = '\0';

            return true;
        }

        arg = arg_end;
    }

    return false;
}
