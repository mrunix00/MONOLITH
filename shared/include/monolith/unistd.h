/* Minimal hosted-GCC system header for MONOLITH's toolchain sysroot. */
#pragma once

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int getpid(void);
int close(int fd);
int access(const char *path, int mode);
int spawn_task(int argc, const char **argv, const int *inherit_rds, int inherit_rd_count);
