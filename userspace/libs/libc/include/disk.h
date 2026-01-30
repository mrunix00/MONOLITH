/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

#define FILE void *

typedef enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
} seek_mode_t;

typedef enum {
    TYPE_FILE,
    TYPE_DIRECTORY,
} file_type_t;

typedef struct
{
    uint64_t size;
    file_type_t type;
} file_stats_t;

FILE file_open(const char *path);

int file_read(FILE file, void *buffer, uint32_t size);

int file_seek(FILE file, uint32_t offset, seek_mode_t mode);

int file_getstats(FILE file, file_stats_t *stats);
