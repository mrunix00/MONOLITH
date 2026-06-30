/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int rsrc_handle_t;

#define RSRC_INVALID_HANDLE (-1)
#define RSRC_PATH_MAX_LEN 4096
#define RSRC_NAME_MAX_LEN 256

#define RSRC_POLL_READ 0x1
#define RSRC_POLL_WRITE 0x2

typedef uint64_t rsrc_id_t;

typedef struct
{
    rsrc_handle_t handle;
    uint64_t events;
} rsrc_poll_t;

typedef enum {
    RSRC_DOMAIN_NULL = 0,
    RSRC_DOMAIN_FILE,
    RSRC_DOMAIN_DEVICE,
    RSRC_DOMAIN_CHANNEL,
    RSRC_DOMAIN_SHM,
    RSRC_DOMAIN_TASK,
    RSRC_DOMAIN_PIPE,
    RSRC_DOMAIN_END,
} rsrc_domain_id_t;

typedef enum {
    RSRC_TYPE_NULL = 0,
    RSRC_TYPE_COLLECTION,
    RSRC_TYPE_RESOURCE,
} rsrc_type_t;

typedef struct
{
    uint64_t size;
    uint64_t child_count;
} rsrc_file_info_t;

typedef struct
{
    uint64_t channel_id;
    uint64_t owner_task_id;
} rsrc_channel_info_t;

typedef struct
{
    uint64_t size;
    uint64_t page_count;
} rsrc_shm_info_t;

typedef enum {
    RSRC_TASK_STATE_RUNNABLE = 0,
    RSRC_TASK_STATE_SLEEPING,
    RSRC_TASK_STATE_EXITING,
} rsrc_task_state_t;

typedef struct
{
    uint64_t task_id;
    uint64_t parent_task_id;
    uint64_t child_count;
    rsrc_task_state_t state;
    char path[RSRC_PATH_MAX_LEN];
    char name[RSRC_NAME_MAX_LEN];
} rsrc_task_info_t;

typedef struct
{
    rsrc_domain_id_t domain_id;
    rsrc_id_t id;
    rsrc_type_t type;
    union {
        void *info;
        rsrc_file_info_t file;
        rsrc_channel_info_t channel;
        rsrc_shm_info_t shm;
        rsrc_task_info_t task;
    };
} rsrc_info_t;

rsrc_handle_t rsmgr_open(const char *path);
int rsmgr_close(rsrc_handle_t handle);
int rsmgr_describe(rsrc_handle_t handle, rsrc_info_t *info);
int rsmgr_read(rsrc_handle_t handle, void *buffer, uint32_t size, uint64_t *out_bytes_read);
int rsmgr_write(rsrc_handle_t handle, const void *buffer, uint32_t size);
int rsmgr_list(rsrc_handle_t handle, void *buffer, uint32_t size);
int rsmgr_seek(rsrc_handle_t handle, int offset, int whence);
long rsmgr_control(rsrc_handle_t handle, uint64_t command_id, void *buffer, uint32_t size);
void *rsmgr_mmap(rsrc_handle_t handle, uint64_t offset, uint64_t length, uint64_t prot);
int rsmgr_poll(const rsrc_poll_t *polls, uint32_t count);
