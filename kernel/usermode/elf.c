/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/debug.h>
#include <kernel/fs/vfs.h>
#include <kernel/klibc/string.h>
#include <kernel/usermode/elf.h>

int parse_elf_header(file_t *file, void *buffer)
{
    file_seek(file, 0, SEEK_SET);
    if (file_read(file, buffer, sizeof(elf_header_t)) < 0) {
        debug_log("[-] I/O Error");
        return -1;
    }

    elf_header_t *header = buffer;
    if (strncmp(header->magic, ELF_MAGIC, sizeof(header->magic)) != 0) {
        debug_log("[-] Invalid ELF Header!\n");
        return -1;
    }

    int bytes;
    if (header->format == ELF_FORMAT_32BIT) {
        bytes = file_read(file, buffer + sizeof(elf_header_t), sizeof(elf32_header_t));
    } else if (header->format == ELF_FORMAT_64BIT) {
        bytes = file_read(file, buffer + sizeof(elf_header_t), sizeof(elf64_header_t));
    } else {
        debug_log("[-] Invalid ELF format!\n");
        return -1;
    }

    if (bytes < 0) {
        debug_log("[-] I/O Error\n");
        return -1;
    }
    return bytes + sizeof(elf_header_t);
}

int parse_elf_program_header(file_t *file, void *buffer, size_t size)
{
    return file_read(file, buffer, size);
}
