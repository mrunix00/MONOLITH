/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

/*
 * USTAR flags.
 * https://wiki.osdev.org/USTAR#Format_Details
 */
typedef enum : char {
    USTAR_NORMAL = '0',
    USTAR_HARDLINK = '1',
    USTAR_SYMLINK = '2',
    USTAR_CHARDEVICE = '3',
    USTAR_BLOCKDEVICE = '4',
    USTAR_DIRECTORY = '5',
    USTAR_FIFO = '6',
} ustar_flag_t;

/*
 * USTAR block structure.
 * https://wiki.osdev.org/USTAR#Format_Details
 */
typedef struct
{
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char modification_time[12];
    char checksum[8];
    ustar_flag_t flag;
    char linkname[100];
    char magic[6];
    char version[2];
    char username[32];
    char groupname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char reserved[12];
} ustar_block_t;
