/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/terminal/kshell.h>
#include <kernel/terminal/terminal.h>

#define KSHELL_BUFFER_SIZE 1024
#define KSHELL_COMMANDS_LIMIT 256
#define KSHELL_ARG_SIZE 32

/*
 * kshell command callback.
 */
typedef void (*kshell_command_t)(int, char **);

/*
 * A structure that contains the name, description and callback
 * for a kshell command.
 */
typedef struct
{
    const char *name;
    const char *desc;
    kshell_command_t command;
} kshell_command_desc_t;

/*
 * Initializes basic kernel commands such as `help`.
 */
void kshell_init();

/*
 * Launches the kernel shell.
 */
void kshell_launch();

/*
 * Registers a new kshell command.
 */
void kshell_register_command(const char *name, const char *desc, kshell_command_t);
