/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/rsmgr/rsmgr.h>
#include <kernel/utils/idalloc.h>

static rsrc_domain_t _rsrc_domains[RSRC_DOMAIN_END] = {0};

static idalloc_t _rsrc_idalloc = {0};
static rsrc_t **_rsrc_list = NULL;
static size_t _rsrc_list_capacity = 0;
static bool _rsmgr_initialized = false;

bool rsmgr_init()
{
    if (_rsmgr_initialized)
        return true;

    if (!idalloc_init(&_rsrc_idalloc, 64))
        return false;
    _rsmgr_initialized = true;
    return true;
}

bool rsmgr_init_domain(rsrc_domain_id_t id, const char *name, rsrc_ops_t *ops)
{
    if (id <= RSRC_DOMAIN_NULL || id >= RSRC_DOMAIN_END || name == NULL || ops == NULL)
        return false;

    if (_rsrc_domains[id].id != RSRC_DOMAIN_NULL) {
        debug_log_fmt("Resource domain %s with id %d is already initialized!", name, id);
        return false;
    }
    _rsrc_domains[id] = (rsrc_domain_t){
        .id = id,
        .name = name,
        .ops = *ops,
        .root_node = NULL,
    };
    return true;
}

static rsrc_t *_rsrc_push_new(rsrc_id_t id)
{
    if (_rsrc_list == NULL) {
        _rsrc_list_capacity = 16;
        _rsrc_list = kmalloc(sizeof(rsrc_t *) * _rsrc_list_capacity);
        if (_rsrc_list == NULL)
            return NULL;
    }
    if (id >= _rsrc_list_capacity) {
        size_t _rsrc_list_new_capacity = id * 2;
        rsrc_t **_rsrc_list_new = krealloc(_rsrc_list, sizeof(rsrc_t *) * _rsrc_list_new_capacity);
        if (_rsrc_list_new == NULL)
            return NULL;
        _rsrc_list = _rsrc_list_new;
        _rsrc_list_capacity = _rsrc_list_new_capacity;
    }

    _rsrc_list[id] = kmalloc(sizeof(rsrc_t));
    if (_rsrc_list[id] == NULL)
        return NULL;
    _rsrc_list[id]->header.id = id;
    _rsrc_list[id]->refcount = 0;
    _rsrc_list[id]->domain = NULL;
    _rsrc_list[id]->ops = NULL;
    _rsrc_list[id]->node = NULL;
    return _rsrc_list[id];
}

rsrc_t *rsmgr_new_resource(rsrc_domain_id_t domain_id, const char *name)
{
    size_t id = idalloc_next(&_rsrc_idalloc);
    if (id == UINT64_MAX)
        return NULL;

    rsrc_t *rsrc = _rsrc_push_new(id);
    if (rsrc == NULL) {
        idalloc_free(&_rsrc_idalloc, id);
        return NULL;
    }

    rsrc->header.domain_id = domain_id;
    rsrc->refcount = 0;
    strncpy(rsrc->header.name, name, RSRC_NAME_MAX_LEN);
    rsrc->header.name[RSRC_NAME_MAX_LEN - 1] = '\0';
    rsrc->domain = &_rsrc_domains[domain_id];
    rsrc->ops = NULL;
    rsrc->node = NULL;

    return rsrc;
}

rsrc_node_t *rsmgr_set_domain_root(rsrc_domain_id_t domain_id, rsrc_t *resource)
{
    if (!debug_assert(resource != NULL))
        return NULL;

    if (domain_id <= RSRC_DOMAIN_NULL || domain_id >= RSRC_DOMAIN_END)
        return NULL;

    rsrc_domain_t *domain = &_rsrc_domains[domain_id];
    if (!debug_assert(domain->id == domain_id))
        return NULL;

    if (!debug_assert(resource->header.type == RSRC_TYPE_COLLECTION))
        return NULL;

    rsrc_node_t *root_node = kmalloc(sizeof(rsrc_node_t));
    if (root_node == NULL)
        return NULL;

    root_node->resource = resource;
    root_node->parent = NULL;
    root_node->next_sibling = NULL;
    root_node->first_child = NULL;

    domain->root_node = root_node;
    resource->node = root_node;
    return root_node;
}

rsrc_node_t *rsmgr_attach_resource(rsrc_node_t *parent, rsrc_t *child)
{
    if (!debug_assert(parent != NULL && child != NULL && parent->resource != NULL))
        return NULL;

    if (!debug_assert(parent->resource->header.type == RSRC_TYPE_COLLECTION))
        return NULL;

    rsrc_node_t *child_node = kmalloc(sizeof(rsrc_node_t));
    if (child_node == NULL)
        return NULL;
    child_node->resource = child;
    child_node->next_sibling = NULL;
    child_node->first_child = NULL;
    child_node->parent = parent;

    if (parent->first_child == NULL) {
        parent->first_child = child_node;
    } else {
        rsrc_node_t *last_child = parent->first_child;
        while (last_child->next_sibling != NULL)
            last_child = last_child->next_sibling;
        last_child->next_sibling = child_node;
    }

    child->node = child_node;
    return child_node;
}

rsrc_status_t rsmgr_list_collection_entries(
    rsrc_t *resource,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || resource->header.type != RSRC_TYPE_COLLECTION || resource->node == NULL
        || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *current = resource->node->first_child;
    uint32_t buffer_offset = 0;
    size_t current_offset = 0;

    while (current != NULL) {
        size_t name_len = strlen(current->resource->header.name);
        size_t entry_size = sizeof(uint32_t) + name_len + 1;

        if (current_offset + entry_size <= cursor) {
            current_offset += entry_size;
            current = current->next_sibling;
            continue;
        }

        if (buffer_offset + entry_size > buffer_len)
            break;

        memcpy((uint8_t *) buffer + buffer_offset, &entry_size, sizeof(uint32_t));
        memcpy(
            (uint8_t *) buffer + buffer_offset + sizeof(uint32_t),
            current->resource->header.name,
            name_len + 1);

        buffer_offset += (uint32_t) entry_size;
        current_offset += entry_size;
        current = current->next_sibling;
    }

    *out_bytes_written = (uint64_t) buffer_offset;
    if (out_next_cursor != NULL)
        *out_next_cursor = cursor + (uint64_t) buffer_offset;
    return RSRC_STATUS_OK;
}

static rsrc_node_t *_find_child_by_name(rsrc_node_t *parent, const char *name, size_t name_len)
{
    if (parent == NULL || name == NULL)
        return NULL;

    for (rsrc_node_t *child = parent->first_child; child != NULL; child = child->next_sibling) {
        if (strlen(child->resource->header.name) == name_len
            && memcmp(child->resource->header.name, name, name_len) == 0)
            return child;
    }

    return NULL;
}

rsrc_status_t rsmgr_attach_resource_at_path(
    rsrc_domain_id_t domain_id, const char *path, rsrc_t *resource, const rsrc_ops_t *collection_ops)
{
    if (domain_id <= RSRC_DOMAIN_NULL || domain_id >= RSRC_DOMAIN_END || path == NULL
        || resource == NULL || collection_ops == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_domain_t *domain = &_rsrc_domains[domain_id];
    if (domain->id != domain_id || domain->root_node == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_node_t *parent = domain->root_node;
    const char *component = path;
    while (*component == '/')
        component++;

    if (*component == '\0')
        return RSRC_ERROR_INVALID_ARGUMENT;

    while (*component != '\0') {
        const char *end = component;
        while (*end != '\0' && *end != '/')
            end++;

        size_t component_len = end - component;
        if (component_len == 0 || component_len >= RSRC_NAME_MAX_LEN)
            return RSRC_ERROR_INVALID_ARGUMENT;

        const char *next = end;
        while (*next == '/')
            next++;

        if (*next == '\0') {
            if (_find_child_by_name(parent, component, component_len) != NULL)
                return RSRC_ERROR_ALREADY_EXISTS;
            strncpy(resource->header.name, component, component_len);
            resource->header.name[component_len] = '\0';
            return rsmgr_attach_resource(parent, resource) == NULL ? RSRC_ERROR_NO_MEMORY
                                                                   : RSRC_STATUS_OK;
        }

        rsrc_node_t *child = _find_child_by_name(parent, component, component_len);
        if (child == NULL) {
            char name[RSRC_NAME_MAX_LEN];
            strncpy(name, component, component_len);
            name[component_len] = '\0';

            rsrc_t *collection = rsmgr_new_resource(domain_id, name);
            if (collection == NULL)
                return RSRC_ERROR_NO_MEMORY;

            collection->header.type = RSRC_TYPE_COLLECTION;
            collection->ops = collection_ops;
            collection->type_state = NULL;
            child = rsmgr_attach_resource(parent, collection);
            if (child == NULL) {
                kfree(collection);
                return RSRC_ERROR_NO_MEMORY;
            }
        } else if (child->resource->header.type != RSRC_TYPE_COLLECTION) {
            return RSRC_ERROR_ALREADY_EXISTS;
        }

        parent = child;
        component = next;
    }

    return RSRC_ERROR_INVALID_ARGUMENT;
}

rsrc_domain_t *rsmgr_get_domain(const char *name)
{
    for (size_t i = 0; i < sizeof(_rsrc_domains) / sizeof(_rsrc_domains[0]); i++) {
        if (_rsrc_domains[i].name != NULL && strcmp(_rsrc_domains[i].name, name) == 0)
            return &_rsrc_domains[i];
    }
    return NULL;
}

/*
 * Parse the full path into domain and path components.
 */
static rsrc_status_t _get_path(
    const char *full_path, rsrc_domain_t **domain_out, char *path_out, size_t buffer_size)
{
    if (full_path == NULL || *full_path == '\0')
        return RSRC_ERROR_INVALID_ARGUMENT;

    /* Check if this is a full path */
    const char *colon_pos = strchr(full_path, ':');
    if (colon_pos == NULL || colon_pos == full_path || *(colon_pos + 1) != '/')
        return RSRC_ERROR_INVALID_ARGUMENT;

    /* Extract the domain name from the scheme prefix. */
    size_t name_len = colon_pos - full_path;
    if (name_len >= RSRC_NAME_MAX_LEN)
        return RSRC_ERROR_OUT_OF_RANGE;

    char domain_name[RSRC_NAME_MAX_LEN];
    strncpy(domain_name, full_path, name_len);
    domain_name[name_len] = '\0';

    *domain_out = rsmgr_get_domain(domain_name);
    if (*domain_out == NULL)
        return RSRC_ERROR_NOT_FOUND;

    /* Skip the scheme prefix (`name:/`). */
    full_path = colon_pos + 2;

    path_out[0] = '\0';
    size_t pos = 0;

    /* Process each component of the path */
    char *component_start = (char *) full_path;
    char *component_end;
    while (*component_start) {
        while (*component_start == '/')
            component_start++;
        if (!*component_start)
            break;

        /* Find end of component */
        component_end = component_start;
        while (*component_end != '\0' && *component_end != '/')
            component_end++;

        size_t component_len = component_end - component_start;

        /* Handle special components */
        if (component_len == 1 && component_start[0] == '.') {
            /* "." - stay in the current directory */
        } else if (component_len == 2 && component_start[0] == '.' && component_start[1] == '.') {
            /* ".." - go up one directory */
            if (pos > 1) { // Make sure we don't go past root
                pos--;     // Remove trailing slash
                while (pos > 1 && path_out[pos - 1] != '/')
                    pos--;
                path_out[pos] = '\0';
            }
        } else {
            /* Regular component, append it */
            if (pos + component_len + 1 >= buffer_size)
                return RSRC_ERROR_OUT_OF_RANGE;

            if (pos > 1 || path_out[0] != '/') // Don't add slash if we're at root
                path_out[pos++] = '/';
            strncpy(&path_out[pos], component_start, component_len);
            pos += component_len;
            path_out[pos] = '\0';
        }

        /* Move to next component */
        component_start = component_end;
    }

    /* If we ended up with an empty path, make sure it's at least "/" */
    if (pos == 0) {
        path_out[pos++] = '/';
        path_out[pos] = '\0';
    }

    return RSRC_STATUS_OK;
}

rsrc_node_t *rsmgr_get_path(const char *path)
{
    rsrc_domain_t *domain;
    char path_out[RSRC_PATH_MAX_LEN];
    if (_get_path(path, &domain, path_out, RSRC_PATH_MAX_LEN) != RSRC_STATUS_OK)
        return NULL;
    return rsmgr_get_relative_path(domain->root_node, path_out);
}

void rsmgr_ref(rsrc_t *resource)
{
    if (resource == NULL)
        return;
    resource->refcount++;
}

static void _rsmgr_destroy_if_unreferenced(rsrc_t *resource)
{
    if (resource == NULL || resource->refcount > 0 || resource->node != NULL)
        return;

    if (resource->ops != NULL && resource->ops->destroy != NULL)
        resource->ops->destroy(resource);

    if (resource->header.id < _rsrc_list_capacity)
        _rsrc_list[resource->header.id] = NULL;
    idalloc_free(&_rsrc_idalloc, resource->header.id);
    kfree(resource);
}

void rsmgr_unref(rsrc_t *resource)
{
    if (resource == NULL)
        return;
    if (!debug_assert(resource->refcount > 0))
        return;
    resource->refcount--;
    _rsmgr_destroy_if_unreferenced(resource);
}

rsrc_status_t rsmgr_normalize_global_path(const char *path, char *path_out, size_t buffer_size)
{
    if (path_out == NULL || buffer_size == 0)
        return RSRC_ERROR_INVALID_ARGUMENT;

    rsrc_domain_t *domain;
    rsrc_status_t result = _get_path(path, &domain, path_out, buffer_size);
    if (result != RSRC_STATUS_OK)
        return result;
    if (domain == NULL || domain->name == NULL)
        return RSRC_ERROR_NOT_FOUND;

    size_t domain_len = strlen(domain->name);
    size_t relative_len = strlen(path_out);
    if (domain_len + 1 + relative_len + 1 > buffer_size)
        return RSRC_ERROR_OUT_OF_RANGE;

    for (size_t i = relative_len + 1; i > 0; i--)
        path_out[domain_len + i] = path_out[i - 1];
    memcpy(path_out, domain->name, domain_len);
    path_out[domain_len] = ':';
    return RSRC_STATUS_OK;
}

rsrc_status_t rsmgr_get_resource_path(const rsrc_t *resource, char *path_out, size_t buffer_size)
{
    if (resource == NULL || path_out == NULL || buffer_size == 0)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->domain == NULL || resource->domain->name == NULL || resource->node == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    const rsrc_node_t *node = resource->node;
    const char *components[RSRC_PATH_MAX_LEN / 2];
    size_t component_lengths[RSRC_PATH_MAX_LEN / 2];
    size_t component_count = 0;

    while (node != NULL && node->parent != NULL) {
        if (component_count >= (RSRC_PATH_MAX_LEN / 2))
            return RSRC_ERROR_OUT_OF_RANGE;
        components[component_count] = node->resource->header.name;
        component_lengths[component_count] = strlen(node->resource->header.name);
        component_count++;
        node = node->parent;
    }

    if (node == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    size_t domain_len = strlen(resource->domain->name);
    if (domain_len + 3 > buffer_size)
        return RSRC_ERROR_OUT_OF_RANGE;

    memcpy(path_out, resource->domain->name, domain_len);
    size_t pos = domain_len;
    path_out[pos++] = ':';
    path_out[pos++] = '/';

    for (size_t i = component_count; i > 0; i--) {
        size_t index = i - 1;
        size_t component_len = component_lengths[index];
        if (component_len == 0)
            continue;
        if (pos + component_len + 1 > buffer_size)
            return RSRC_ERROR_OUT_OF_RANGE;
        memcpy(path_out + pos, components[index], component_len);
        pos += component_len;
        if (index != 0)
            path_out[pos++] = '/';
    }

    if (pos >= buffer_size)
        return RSRC_ERROR_OUT_OF_RANGE;
    path_out[pos] = '\0';
    return RSRC_STATUS_OK;
}

rsrc_node_t *rsmgr_get_relative_path(rsrc_node_t *parent, const char *path)
{
    if (parent == NULL)
        return NULL;

    while (*path == '/')
        path++;
    if (*path == '\0')
        return parent;

    rsrc_node_t *current = parent;
    while (*path) {
        const char *end = path;
        while (*end != '\0' && *end != '/')
            end++;

        size_t token_len = end - path;
        if (token_len == 0) {
            path = end;
            while (*path == '/')
                path++;
            continue;
        }

        if (current->resource->header.type != RSRC_TYPE_COLLECTION)
            return NULL;

        rsrc_node_t *child = current->first_child;
        rsrc_node_t *found = NULL;
        while (child != NULL) {
            if (strlen(child->resource->header.name) == token_len
                && memcmp(child->resource->header.name, path, token_len) == 0) {
                found = child;
                break;
            }
            child = child->next_sibling;
        }

        if (found == NULL)
            return NULL;

        current = found;
        path = end;

        while (*path == '/')
            path++;
    }

    return current;
}

/*
 * Handle table implementation.
 */

#define RSRC_HANDLE_TABLE_INITIAL_CAPACITY 16

rsrc_status_t rsmgr_handle_table_init(rsrc_handle_table_t *table)
{
    if (table == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    table->entries = kmalloc(sizeof(rsrc_handle_entry_t) * RSRC_HANDLE_TABLE_INITIAL_CAPACITY);
    if (table->entries == NULL)
        return RSRC_ERROR_NO_MEMORY;

    memset(table->entries, 0, sizeof(rsrc_handle_entry_t) * RSRC_HANDLE_TABLE_INITIAL_CAPACITY);
    table->count = 0;
    table->capacity = RSRC_HANDLE_TABLE_INITIAL_CAPACITY;
    return RSRC_STATUS_OK;
}

static int _handle_table_ensure_capacity(rsrc_handle_table_t *table, size_t min_capacity)
{
    if (table->capacity >= min_capacity)
        return 0;

    size_t new_capacity = table->capacity;
    while (new_capacity < min_capacity)
        new_capacity *= 2;

    rsrc_handle_entry_t *new_entries
        = krealloc(table->entries, sizeof(rsrc_handle_entry_t) * new_capacity);
    if (new_entries == NULL)
        return -1;

    /* Zero out new entries */
    size_t old_size = table->capacity * sizeof(rsrc_handle_entry_t);
    size_t new_size = new_capacity * sizeof(rsrc_handle_entry_t);
    memset((uint8_t *) new_entries + old_size, 0, new_size - old_size);

    table->entries = new_entries;
    table->capacity = new_capacity;
    return 0;
}

int rsmgr_handle_table_alloc(rsrc_handle_table_t *table, rsrc_t *resource)
{
    if (table == NULL || resource == NULL)
        return -1;

    /* Find first free slot */
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].resource == NULL) {
            table->entries[i].resource = resource;
            table->entries[i].offset = 0;
            resource->refcount++;
            if (i >= table->count)
                table->count = i + 1;
            return (int) i;
        }
    }

    /* No free slot, expand */
    size_t new_index = table->capacity;
    if (_handle_table_ensure_capacity(table, new_index + 1) < 0)
        return -1;

    table->entries[new_index].resource = resource;
    table->entries[new_index].offset = 0;
    resource->refcount++;
    table->count = new_index + 1;
    return (int) new_index;
}

rsrc_status_t rsmgr_handle_table_inherit(
    rsrc_handle_table_t *table, int fd, const rsrc_handle_entry_t *source)
{
    if (table == NULL || source == NULL || source->resource == NULL || fd < 0)
        return RSRC_ERROR_INVALID_ARGUMENT;

    if (_handle_table_ensure_capacity(table, (size_t) fd + 1) < 0)
        return RSRC_ERROR_NO_MEMORY;

    if (table->entries[fd].resource != NULL)
        return RSRC_ERROR_ALREADY_EXISTS;

    table->entries[fd].resource = source->resource;
    table->entries[fd].offset = source->offset;
    source->resource->refcount++;
    if ((size_t) fd >= table->count)
        table->count = (size_t) fd + 1;

    return RSRC_STATUS_OK;
}

rsrc_handle_entry_t *rsmgr_handle_table_get(rsrc_handle_table_t *table, int fd)
{
    if (table == NULL || fd < 0 || (size_t) fd >= table->capacity)
        return NULL;

    if (table->entries[fd].resource == NULL)
        return NULL;

    return &table->entries[fd];
}

rsrc_status_t rsmgr_handle_table_close(rsrc_handle_table_t *table, int fd)
{
    if (table == NULL || fd < 0 || (size_t) fd >= table->capacity)
        return RSRC_ERROR_BAD_HANDLE;

    rsrc_handle_entry_t *entry = &table->entries[fd];
    if (entry->resource == NULL)
        return RSRC_ERROR_BAD_HANDLE;

    if (entry->resource->ops != NULL && entry->resource->ops->close_handle != NULL)
        entry->resource->ops->close_handle(entry->resource, NULL);

    rsrc_t *resource = entry->resource;

    /* Drop reference */
    resource->refcount--;
    entry->resource = NULL;
    entry->offset = 0;
    _rsmgr_destroy_if_unreferenced(resource);

    /* Shrink count if closing last entries */
    while (table->count > 0 && table->entries[table->count - 1].resource == NULL)
        table->count--;

    return RSRC_STATUS_OK;
}

void rsmgr_handle_table_destroy(rsrc_handle_table_t *table)
{
    if (table->entries != NULL) {
        for (size_t i = 0; i < table->capacity; i++) {
            rsrc_t *current_rsrc = table->entries[i].resource;
            if (current_rsrc != NULL) {
                if (current_rsrc->ops != NULL && current_rsrc->ops->close_handle != NULL)
                    current_rsrc->ops->close_handle(current_rsrc, NULL);
                current_rsrc->refcount--;
                table->entries[i].resource = NULL;
                _rsmgr_destroy_if_unreferenced(current_rsrc);
            }
        }
        kfree(table->entries);
        table->entries = NULL;
    }

    table->count = 0;
    table->capacity = 0;
}

rsrc_t *rsmgr_open(const char *path)
{
    rsrc_domain_t *domain;
    char path_out[RSRC_PATH_MAX_LEN];
    if (_get_path(path, &domain, path_out, RSRC_PATH_MAX_LEN) != RSRC_STATUS_OK)
        return NULL;

    if (domain == NULL || domain->ops.open == NULL)
        return NULL;

    rsrc_t *resource = NULL;
    void *handle_state = NULL;
    rsrc_status_t result = domain->ops.open(domain, path_out, &resource, &handle_state);
    if (result != RSRC_STATUS_OK || resource == NULL)
        return NULL;

    return resource;
}

rsrc_t *rsmgr_lookup(rsrc_t *collection, const char *path)
{
    if (collection == NULL || collection->domain == NULL || collection->domain->ops.lookup == NULL)
        return NULL;

    rsrc_node_t *parent = collection->node;
    if (parent == NULL)
        return NULL;

    rsrc_t *resource = NULL;
    void *handle_state = NULL;
    rsrc_status_t result = collection->domain->ops.lookup(parent, path, &resource, &handle_state);
    if (result != RSRC_STATUS_OK || resource == NULL)
        return NULL;

    return resource;
}

static rsrc_status_t _rsmgr_create_in_domain(
    const char *path, rsrc_domain_id_t domain_id, rsrc_type_t type, rsrc_t **out_resource)
{
    if (path == NULL || out_resource == NULL
        || (type != RSRC_TYPE_COLLECTION && type != RSRC_TYPE_RESOURCE))
        return RSRC_ERROR_INVALID_ARGUMENT;

    size_t path_len = strlen(path);
    if (path_len == 0 || path_len >= RSRC_PATH_MAX_LEN)
        return RSRC_ERROR_INVALID_ARGUMENT;

    char parent_path[RSRC_PATH_MAX_LEN];
    memcpy(parent_path, path, path_len + 1);

    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == NULL || last_slash[1] == '\0')
        return RSRC_ERROR_INVALID_ARGUMENT;

    size_t name_len = strlen(last_slash + 1);
    if (name_len >= RSRC_NAME_MAX_LEN)
        return RSRC_ERROR_OUT_OF_RANGE;

    char name[RSRC_NAME_MAX_LEN];
    memcpy(name, last_slash + 1, name_len + 1);

    if (last_slash > parent_path && last_slash[-1] == ':')
        last_slash[1] = '\0';
    else
        *last_slash = '\0';

    rsrc_t *collection = rsmgr_open(parent_path);
    if (collection == NULL)
        return RSRC_ERROR_NOT_FOUND;
    if (collection->header.domain_id != domain_id)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (collection->ops == NULL || collection->ops->create == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;

    return collection->ops->create(collection, NULL, name, type, out_resource);
}

rsrc_status_t rsmgr_create_file(const char *path, rsrc_t **out_resource)
{
    return _rsmgr_create_in_domain(path, RSRC_DOMAIN_FILE, RSRC_TYPE_RESOURCE, out_resource);
}

rsrc_status_t rsmgr_create_directory(const char *path, rsrc_t **out_resource)
{
    return _rsmgr_create_in_domain(path, RSRC_DOMAIN_FILE, RSRC_TYPE_COLLECTION, out_resource);
}

rsrc_status_t rsmgr_describe(rsrc_t *resource, rsrc_info_t *out_info)
{
    if (resource == NULL || out_info == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    memset(out_info, 0, sizeof(*out_info));
    out_info->domain_id = resource->header.domain_id;
    out_info->id = resource->header.id;
    out_info->type = resource->header.type;

    if (resource->ops != NULL && resource->ops->describe != NULL)
        return resource->ops->describe(resource, out_info);

    return RSRC_STATUS_OK;
}

rsrc_status_t rsmgr_seek(
    rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset)
{
    if (resource == NULL || out_offset == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->seek == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->seek(resource, handle_state, offset, whence, out_offset);
}

rsrc_status_t rsmgr_list(
    rsrc_t *resource,
    void *handle_state,
    uint64_t cursor,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_next_cursor,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->list == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->list(
        resource, handle_state, cursor, buffer, buffer_len, out_next_cursor, out_bytes_written);
}

rsrc_status_t rsmgr_read(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || buffer == NULL || out_bytes_read == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->read == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->read(resource, handle_state, buffer, buffer_len, out_bytes_read);
}

rsrc_status_t rsmgr_write(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->write == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->write(resource, handle_state, buffer, buffer_len, out_bytes_written);
}

rsrc_status_t rsmgr_mmap(
    rsrc_t *resource,
    void *handle_state,
    uint64_t offset,
    uint64_t length,
    uint64_t prot,
    uint64_t *out_address)
{
    if (resource == NULL || out_address == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->mmap == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->mmap(resource, handle_state, offset, length, prot, out_address);
}

rsrc_status_t rsmgr_poll(
    rsrc_t *resource, void *handle_state, uint64_t requested_events, uint64_t *out_ready_events)
{
    if (resource == NULL || out_ready_events == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->poll == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->poll(resource, handle_state, requested_events, out_ready_events);
}

rsrc_status_t rsmgr_remove(rsrc_t *resource, void *handle_state, const char *name)
{
    if (resource == NULL || name == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->remove == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->remove(resource, handle_state, name);
}

rsrc_status_t rsmgr_control(
    rsrc_t *resource,
    void *handle_state,
    uint64_t command_id,
    void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || buffer == NULL || out_bytes_written == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;
    if (resource->ops == NULL || resource->ops->control == NULL)
        return RSRC_ERROR_NOT_SUPPORTED;
    return resource->ops->control(
        resource, handle_state, command_id, buffer, buffer_len, buffer, buffer_len, out_bytes_written);
}
