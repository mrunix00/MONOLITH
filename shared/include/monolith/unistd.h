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
int mkdir(const char *path, unsigned mode);
int file_create(const char *path);

typedef struct
{
    int current_descriptor;
    int target_descriptor;
} task_create_inherit_t;

int task_create(int argc, const char **argv, const task_create_inherit_t *inherit, int inherit_count);
