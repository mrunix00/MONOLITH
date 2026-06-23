/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/debug.h>
#include <kernel/klibc/string.h>
#include <kernel/rsmgr/rsmgr.h>
#include <kernel/tasking/elf.h>
#include <shared/include/monolith/stdio.h>

int parse_elf_header(rsrc_t *executable, void *buffer)
{
    if (executable == NULL)
        return -1;

    uint64_t offset = 0;
    uint64_t bytes_read = 0;
    if (rsmgr_seek(executable, &offset, 0, SEEK_SET, &offset) != RSRC_STATUS_OK
        || rsmgr_read(executable, &offset, buffer, sizeof(elf_header_t), &bytes_read)
               != RSRC_STATUS_OK
        || bytes_read < sizeof(elf_header_t)) {
        debug_log("I/O Error");
        return -1;
    }
    offset += bytes_read;

    elf_header_t *header = buffer;
    if (strncmp(header->magic, ELF_MAGIC, sizeof(header->magic)) != 0) {
        debug_log("Invalid ELF Header!\n");
        return -1;
    }

    size_t rest_size = 0;
    if (header->format == ELF_FORMAT_32BIT) {
        rest_size = sizeof(elf32_header_t) - sizeof(elf_header_t);
    } else if (header->format == ELF_FORMAT_64BIT) {
        rest_size = sizeof(elf64_header_t) - sizeof(elf_header_t);
    } else {
        debug_log("Invalid ELF format!\n");
        return -1;
    }

    if (rsmgr_read(
            executable, &offset, (uint8_t *) buffer + sizeof(elf_header_t), rest_size, &bytes_read)
            != RSRC_STATUS_OK
        || bytes_read < rest_size) {
        debug_log("I/O Error\n");
        return -1;
    }
    return (int) (sizeof(elf_header_t) + rest_size);
}

int parse_elf_program_header(rsrc_t *resource, uint64_t offset, void *buffer, size_t size)
{
    if (resource == NULL)
        return -1;

    uint64_t bytes_read = 0;
    if (rsmgr_seek(resource, &offset, (int64_t) offset, SEEK_SET, &offset) != RSRC_STATUS_OK
        || rsmgr_read(resource, &offset, buffer, size, &bytes_read) != RSRC_STATUS_OK
        || bytes_read < size)
        return -1;

    return (int) bytes_read;
}
