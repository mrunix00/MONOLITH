/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <kernel/fs/vfs.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ELF magic number (0x7F followed by 'ELF' in ASCII).
 * https://wiki.osdev.org/ELF#ELF_Header
 */
#define ELF_MAGIC \
    "\x7f" \
    "ELF"

/*
 * ELF format (32-bit or 64-bit).
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef enum : uint8_t {
    ELF_FORMAT_32BIT = 1,
    ELF_FORMAT_64BIT = 2,
} elf_format_t;

/*
 * ELF endianess (little or big endian).
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef enum : uint8_t {
    ELF_LITTLE_ENDIAN = 1,
    ELF_BIG_ENDIAN = 2,
} elf_endian_t;

/*
 * ELF type (none, relocatable, executable, dynamic, core).
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef enum : uint16_t {
    ELF_TYPE_NONE = 0,
    ELF_TYPE_REL = 1,
    ELF_TYPE_EXEC = 2,
    ELF_TYPE_DYN = 3,
    ELF_TYPE_CORE = 4,
} elf_type_t;

/*
 * ELF instruction set architecture (ISA).
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef enum : uint16_t {
    ELF_ISA_NONE = 0x00,
    ELF_ISA_SPARC = 0x02,
    ELF_ISA_X86 = 0x03,
    ELF_ISA_MISP = 0x08,
    ELF_ISA_PPC = 0x14,
    ELF_ISA_ARM = 0x28,
    ELF_ISA_SUPERH = 0x2A,
    ELF_ISA_IA64 = 0x32,
    ELF_ISA_X86_64 = 0x3E,
    ELF_ISA_AARCH64 = 0xB7,
    ELF_ISA_RISCV = 0xF3,
} elf_isa_t;

/*
 * ELF header structure.
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef struct __attribute__((packed))
{
    char magic[4];          /* ELF magic number (0x7F followed by 'ELF' in ASCII) */
    elf_format_t format;    /* ELF format (32-bit or 64-bit) */
    elf_endian_t endian;    /* Endianess (little or big endian) */
    uint8_t header_version; /* ELF header version */
    uint8_t osabi;          /* OS ABI (0 for System V ABI) */
    uint8_t unused[8];      /* Unused bytes for padding */
    elf_type_t type;        /* ELF File type */
    elf_isa_t isa;          /* Instruction set architecture */
    char version[4];        /* ELF version */
} elf_header_t;

/*
 * ELF header structure for 64-bit ELF files.
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef struct __attribute__((packed))
{
    elf_header_t header;
    uint64_t entry_offset;    /* Program entry point offset */
    uint64_t pht_offset;      /* Program header table offset */
    uint64_t sht_offset;      /* Section header table offset */
    uint8_t flags[4];         /* Architecture-dependent flags */
    uint16_t elf_header_size; /* Size of this header in bytes */
    uint16_t pht_entry_size;  /* Size of each entry in the program header table */
    uint16_t pht_entry_count; /* Number of entries in the program header table */
    uint16_t sht_entry_size;  /* Size of each entry in the section header table */
    uint16_t sht_entry_count; /* Number of entries in the section header table */
    uint16_t shst_index;      /* Index of the section header string table */
} elf64_header_t;

/*
 * ELF header structure for 32-bit ELF files.
 * https://wiki.osdev.org/ELF#ELF_Header
 */
typedef struct __attribute__((packed))
{
    elf_header_t header;
    uint32_t entry_offset;    /* Program entry point offset */
    uint32_t pht_offset;      /* Program header table offset */
    uint32_t sht_offset;      /* Section header table offset */
    uint8_t flags[4];         /* Architecture-dependent flags */
    uint16_t elf_header_size; /* Size of this header in bytes */
    uint16_t pht_entry_size;  /* Size of each entry in the program header table */
    uint16_t pht_entry_count; /* Number of entries in the program header table */
    uint16_t sht_entry_size;  /* Size of each entry in the section header table */
    uint16_t sht_entry_count; /* Number of entries in the section header table */
    uint16_t shst_index;      /* Index of the section header string table */
} elf32_header_t;

/*
 * Program section header type.
 * https://wiki.osdev.org/ELF#Program_header
 */
typedef enum : uint32_t {
    SECTION_TYPE_NULL = 0,    /* Null (ignore the entry) */
    SECTION_TYPE_LOAD = 1,    /* Loadable section */
    SECTION_TYPE_DYNAMIC = 2, /* Dynamically linked section */
    SECTION_TYPE_INTERP = 3,  /* Contains a file path to an executable to use as an interpreter */
    SECTION_TYPE_NOTE = 4,    /* Note section */
} elf_phs_type_t;

/*
 * Program section header flags.
 * https://wiki.osdev.org/ELF#Program_header
 */
typedef enum : uint32_t {
    SECTION_FLAG_READ = 1 << 0,
    SECTION_FLAG_WRITE = 1 << 1,
    SECTION_FLAG_EXEC = 1 << 2,
} elf_phs_flags_t;

/*
 * Program section header for 64-bit ELF files.
 */
typedef struct __attribute__((packed))
{
    elf_phs_type_t type;          /* Type of section */
    elf_phs_flags_t flags;        /* Section flags */
    uint64_t section_offset;      /* Offset of the section in the file */
    uint64_t section_vaddr;       /* Virtual address of the section */
    uint64_t section_paddr;       /* Physical address of the section */
    uint64_t section_file_size;   /* Size of the section in the file */
    uint64_t section_memory_size; /* Size of the section in memory */
    uint64_t section_align;       /* Required alignment for the section */
} elf64_psh_t;

/*
 * Program section header for 32-bit ELF files.
 */
typedef struct __attribute__((packed))
{
    elf_phs_type_t type;          /* Type of section */
    elf_phs_flags_t flags;        /* Section flags */
    uint32_t section_offset;      /* Offset of the section in the file */
    uint32_t section_vaddr;       /* Virtual address of the section */
    uint32_t section_paddr;       /* Physical address of the section */
    uint32_t section_file_size;   /* Size of the section in the file */
    uint32_t section_memory_size; /* Size of the section in memory */
    uint32_t section_align;       /* Required alignment for the section */
} elf32_psh_t;

/*
 * Parse an ELF file header.
 * Returns read bytes on success, -1 on failure.
 */
int parse_elf_header(file_t *file, void *buffer);

/*
 * Parse an ELF program header.
 * Returns read bytes on success, -1 on failure.
 */
int parse_elf_program_header(file_t *file, void *buffer, size_t size);
