/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RSRC_PATH_MAX_LEN 4096
#define RSRC_NAME_MAX_LEN 256
#define RSRC_HANDLE_TABLE_INITIAL_CAPACITY 16

#define RSRC_POLL_READ 0x1
#define RSRC_POLL_WRITE 0x2

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
    RSRC_STATUS_OK = 0,
    RSRC_ERROR = -1,
    RSRC_ERROR_INVALID_ARGUMENT = -2,
    RSRC_ERROR_NOT_FOUND = -3,
    RSRC_ERROR_NOT_SUPPORTED = -4,
    RSRC_ERROR_NO_MEMORY = -5,
    RSRC_ERROR_ALREADY_EXISTS = -6,
    RSRC_ERROR_BAD_HANDLE = -7,
    RSRC_ERROR_OUT_OF_RANGE = -8,
    RSRC_ERROR_BUSY = -9,
    RSRC_ERROR_WOULD_BLOCK = -10,
    RSRC_ERROR_PERMISSION_DENIED = -11,
    RSRC_ERROR_IO = -12,
} rsrc_status_t;

typedef enum {
    RSRC_TYPE_NULL = 0,
    RSRC_TYPE_COLLECTION,
    RSRC_TYPE_RESOURCE,
} rsrc_type_t;

typedef uint64_t rsrc_id_t;
typedef uint64_t rsrc_refcount_t;
typedef int64_t rsrc_handle_t;
typedef struct _rsrc rsrc_t;
typedef struct _rsrc_node rsrc_node_t;
typedef struct _rsrc_domain rsrc_domain_t;

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
        rsrc_file_info_t file;
        rsrc_channel_info_t channel;
        rsrc_shm_info_t shm;
        rsrc_task_info_t task;
    };
} rsrc_info_t;

typedef struct
{
    rsrc_t *resource;
    uint64_t offset;
} rsrc_handle_entry_t;

typedef struct
{
    rsrc_handle_entry_t *entries;
    size_t count;
    size_t capacity;
} rsrc_handle_table_t;

typedef struct
{
    rsrc_domain_id_t domain_id;
    rsrc_id_t id;
    rsrc_type_t type;
    char name[RSRC_NAME_MAX_LEN];
} rsrc_header_t;

typedef struct
{
    rsrc_status_t (*open)(
        rsrc_domain_t *domain, const char *path, rsrc_t **out_resource, void **out_handle_state);

    rsrc_status_t (*lookup)(
        rsrc_node_t *parent, const char *path, rsrc_t **out_resource, void **out_handle_state);

    rsrc_status_t (*dup_handle)(
        rsrc_t *resource, void *old_handle_state, void **out_new_handle_state);

    void (*close_handle)(rsrc_t *resource, void *handle_state);

    void (*destroy)(rsrc_t *resource);

    rsrc_status_t (*describe)(rsrc_t *resource, rsrc_info_t *out_info);

    rsrc_status_t (*seek)(
        rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset);

    rsrc_status_t (*list)(
        rsrc_t *resource,
        void *handle_state,
        uint64_t cursor,
        void *buffer,
        uint64_t buffer_len,
        uint64_t *out_next_cursor,
        uint64_t *out_bytes_written);

    rsrc_status_t (*read)(
        rsrc_t *resource,
        void *handle_state,
        void *buffer,
        uint64_t buffer_len,
        uint64_t *out_bytes_read);

    rsrc_status_t (*write)(
        rsrc_t *resource,
        void *handle_state,
        const void *buffer,
        uint64_t buffer_len,
        uint64_t *out_bytes_written);

    rsrc_status_t (*mmap)(
        rsrc_t *resource,
        void *handle_state,
        uint64_t offset,
        uint64_t length,
        uint64_t prot,
        uint64_t *out_address);

    rsrc_status_t (*poll)(
        rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events);

    rsrc_status_t (*create)(
        rsrc_t *resource,
        void *handle_state,
        const char *name,
        rsrc_type_t type,
        rsrc_t **out_resource);

    rsrc_status_t (*remove)(rsrc_t *resource, void *handle_state, const char *name);

    rsrc_status_t (*control)(
        rsrc_t *resource,
        void *handle_state,
        uint64_t command_id,
        const void *in,
        uint64_t in_len,
        void *out,
        uint64_t out_len,
        uint64_t *out_bytes_written);
} rsrc_ops_t;

struct _rsrc_domain
{
    rsrc_domain_id_t id;
    const char *name;
    rsrc_ops_t ops;
    rsrc_node_t *root_node;
};

struct _rsrc
{
    rsrc_header_t header;
    rsrc_refcount_t refcount;
    rsrc_domain_t *domain;
    const rsrc_ops_t *ops;
    rsrc_node_t *node;
    void *type_state;
};

struct _rsrc_node
{
    rsrc_t *resource;
    rsrc_node_t *parent;
    rsrc_node_t *next_sibling;
    rsrc_node_t *first_child;
};

/*
 * Initializes the resource manager.
 * Returns `true` on success, `false` otherwise.
 */
bool rsmgr_init();

/*
 * Initializes a resource domain with the given ID, name, and operations.
 * Returns `true` on success, `false` otherwise.
 */
bool rsmgr_init_domain(rsrc_domain_id_t id, const char *name, rsrc_ops_t *ops);

/*
 * Creates a new resource in the given domain with the given name.
 * Returns the new resource on success, `NULL` otherwise.
 */
rsrc_t *rsmgr_new_resource(rsrc_domain_id_t domain_id, const char *name);

/*
 * Attaches a resource as the root node for a domain.
 * Returns the new root node on success, `NULL` otherwise.
 */
rsrc_node_t *rsmgr_set_domain_root(rsrc_domain_id_t domain_id, rsrc_t *resource);

/*
 * Attaches a resource to a node in the global resources tree.
 * Returns the new node on success, `NULL` otherwise.
 */
rsrc_node_t *rsmgr_attach_resource(rsrc_node_t *parent, rsrc_t *resource);

/*
 * Attaches a resource under a domain root at a relative path, creating intermediate
 * collections when needed.
 */
rsrc_status_t rsmgr_attach_resource_at_path(
    rsrc_domain_id_t domain_id,
    const char *path,
    rsrc_t *resource,
    const rsrc_ops_t *collection_ops);

/*
 * Lists the child names of a collection using the directory entry format.
 */
rsrc_status_t rsmgr_list_collection_entries(
    rsrc_t *resource,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written);

/*
 * Returns the resource domain with the given name, or `NULL` if not found.
 */
rsrc_domain_t *rsmgr_get_domain(const char *name);

/*
 * Finds the resource node for the given absolute path.
 * Returns the node when found, `NULL` otherwise.
 */
rsrc_node_t *rsmgr_get_path(const char *path);

/*
 * Normalizes a global path into canonical `domain:/path` form.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_normalize_global_path(const char *path, char *path_out, size_t buffer_size);

/*
 * Finds the resource node for the given relative path, starting from the given parent node.
 * Returns the node when found, `NULL` otherwise.
 */
rsrc_node_t *rsmgr_get_relative_path(rsrc_node_t *parent, const char *path);

/*
 * Initializes a handle table.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_handle_table_init(rsrc_handle_table_t *table);

/*
 * Allocates a new handle for the given resource.
 * Returns the fd number on success, -1 on failure.
 */
int rsmgr_handle_table_alloc(rsrc_handle_table_t *table, rsrc_t *resource);

/*
 * Inherits an existing handle entry into a specific fd slot.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_handle_table_inherit(
    rsrc_handle_table_t *table, int fd, const rsrc_handle_entry_t *source);

/*
 * Gets the handle entry for the given fd.
 * Returns the entry pointer on success, NULL if invalid.
 */
rsrc_handle_entry_t *rsmgr_handle_table_get(rsrc_handle_table_t *table, int fd);

/*
 * Closes the handle for the given fd, dropping the resource reference.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_handle_table_close(rsrc_handle_table_t *table, int fd);

/*
 * Destroys the handle table, closing all handles.
 */
void rsmgr_handle_table_destroy(rsrc_handle_table_t *table);

/*
 * Opens a resource by global path (e.g., "file:/path/to/resource").
 * Returns the resource on success, NULL on failure.
 */
rsrc_t *rsmgr_open(const char *path);

/*
 * Bumps or drops a resource reference held outside handle tables.
 */
void rsmgr_ref(rsrc_t *resource);
void rsmgr_unref(rsrc_t *resource);

/*
 * Builds the canonical absolute path for an attached resource.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_get_resource_path(const rsrc_t *resource, char *path_out, size_t buffer_size);

/*
 * Looks up a child resource from a collection handle.
 * Returns the child resource on success, NULL on failure.
 */
rsrc_t *rsmgr_lookup(rsrc_t *collection, const char *path);

/*
 * Creates a file resource by global path (e.g., "file:/path/to/resource").
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_create_file(const char *path, rsrc_t **out_resource);

/*
 * Describes a resource.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_describe(rsrc_t *resource, rsrc_info_t *out_info);

/*
 * Seeks within a resource using handle-local state.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_seek(
    rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset);

/*
 * Lists entries from a collection resource.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_list(
    rsrc_t *resource,
    void *handle_state,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written);

/*
 * Reads from a resource using handle-local state.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_read(
    rsrc_t *resource,
    void *handle_state,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_read);

/*
 * Writes to a resource using handle-local state.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_write(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written);

/*
 * Maps a resource into the caller's address space.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_mmap(
    rsrc_t *resource,
    void *handle_state,
    uint64_t offset,
    uint64_t length,
    uint64_t prot,
    uint64_t *out_address);

/*
 * Polls a resource for readiness.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_poll(
    rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events);

/*
 * Removes a child entry from a collection resource.
 * Returns `RSRC_STATUS_OK` on success.
 */
rsrc_status_t rsmgr_remove(rsrc_t *resource, void *handle_state, const char *name);

/*
 * Dispatches a type-specific control operation.
 * Returns a non-negative status on success, negative on failure.
 */
rsrc_status_t rsmgr_control(
    rsrc_t *resource,
    void *handle_state,
    uint64_t command_id,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written);
