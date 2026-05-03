/* Minimal hosted-GCC system header for MONOLITH's toolchain sysroot. */
#pragma once

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_TRUNC 01000
#define O_APPEND 02000

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_SETLKW 7

struct flock {
    short l_type;
    short l_whence;
    long l_start;
    long l_len;
    int l_pid;
};

int fcntl(int fd, int cmd, ...);
int open(const char *path, int oflag, ...);
