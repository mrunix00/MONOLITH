/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <libs/limine-protocol/include/limine.h>
#include <stdbool.h>

typedef struct _cmdline_arg
{
    char *key;
    char *value;
    struct _cmdline_arg *next;
} cmdline_arg_t;

cmdline_arg_t *load_kernel_args(struct limine_executable_cmdline_response *);

char *get_kernel_arg(cmdline_arg_t *, const char *);
