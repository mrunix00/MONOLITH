/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/devices/device_domain.h>
#include <kernel/devices/framebuffer.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task.h>
#include <shared/include/monolith/device/framebuffer.h>
#include <shared/include/monolith/stdio.h>
#include <stdint.h>

static framebuffer_t _framebuffers[MAX_FRAMEBUFFERS];
static uint8_t _framebuffer_count = 0;
static bool _framebuffer_devices_ready = false;

typedef struct
{
    framebuffer_t framebuffer;
    uintptr_t phys_addr;
    size_t size;
} _framebuffer_device_state_t;

static _framebuffer_device_state_t _framebuffer_device_states[MAX_FRAMEBUFFERS];

static int _build_framebuffer_device_path(char *buffer, size_t buffer_size, uint8_t index)
{
    static const char prefix[] = "/display/framebuffer";
    size_t prefix_len = sizeof(prefix) - 1;
    if (buffer == NULL || buffer_size <= prefix_len)
        return -1;

    memcpy(buffer, prefix, prefix_len);

    char digits[4];
    size_t digits_len = 0;
    do {
        digits[digits_len++] = (char) ('0' + (index % 10));
        index /= 10;
    } while (index != 0 && digits_len < sizeof(digits));

    if (prefix_len + digits_len + 1 > buffer_size)
        return -1;

    for (size_t i = 0; i < digits_len; i++)
        buffer[prefix_len + i] = digits[digits_len - i - 1];
    buffer[prefix_len + digits_len] = '\0';
    return 0;
}

static rsrc_status_t _framebuffer_seek_op(
    rsrc_t *resource, void *handle_state, int64_t offset, uint64_t whence, uint64_t *out_offset)
{
    if (resource == NULL || handle_state == NULL || out_offset == NULL
        || resource->type_state == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    _framebuffer_device_state_t *state = (_framebuffer_device_state_t *) resource->type_state;
    int64_t base = 0;

    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        base = (int64_t) *(uint64_t *) handle_state;
        break;
    case SEEK_END:
        base = (int64_t) state->size;
        break;
    default:
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    int64_t target = base + offset;
    if (target < 0)
        target = 0;

    *out_offset = (uint64_t) target;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _framebuffer_read_op(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_read == NULL
        || resource->type_state == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _framebuffer_device_state_t *state = (_framebuffer_device_state_t *) resource->type_state;
    uint64_t offset = *(uint64_t *) handle_state;
    if (offset >= state->size) {
        *out_bytes_read = 0;
        return RSRC_STATUS_OK;
    }

    if (offset + buffer_len > state->size)
        buffer_len = state->size - offset;

    memcpy(buffer, (uint8_t *) state->framebuffer.address + offset, (size_t) buffer_len);
    *out_bytes_read = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _framebuffer_write_op(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || handle_state == NULL || buffer == NULL || out_bytes_written == NULL
        || resource->type_state == NULL) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _framebuffer_device_state_t *state = (_framebuffer_device_state_t *) resource->type_state;
    uint64_t offset = *(uint64_t *) handle_state;
    if (offset >= state->size) {
        *out_bytes_written = 0;
        return RSRC_STATUS_OK;
    }

    if (offset + buffer_len > state->size)
        buffer_len = state->size - offset;

    memcpy((uint8_t *) state->framebuffer.address + offset, buffer, (size_t) buffer_len);
    *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _framebuffer_mmap_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t offset,
    uint64_t length,
    uint64_t prot,
    uint64_t *out_address)
{
    if (resource == NULL || handle_state == NULL || out_address == NULL
        || resource->type_state == NULL || length == 0) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    _framebuffer_device_state_t *state = (_framebuffer_device_state_t *) resource->type_state;
    if (offset != 0 || length > state->size)
        return RSRC_ERROR_OUT_OF_RANGE;

    task_t *current = task_get_current();
    if (current == NULL)
        return RSRC_ERROR;

    size_t page_count = PAGE_UP(length) / PAGE_SIZE;
    uintptr_t virt_addr = task_find_free_vaddr(current, page_count);
    if (virt_addr == 0)
        return RSRC_ERROR_NO_MEMORY;

    uint64_t pt_flags = PTFLAG_P | PTFLAG_US | PTFLAG_PAT;
    if (prot & ALLOC_PAGES_FLAG_RW)
        pt_flags |= PTFLAG_RW;
    if (!(prot & ALLOC_PAGES_FLAG_EXEC))
        pt_flags |= PTFLAG_XD;

    if (task_map(current, virt_addr, state->phys_addr, page_count, pt_flags, false) < 0)
        return RSRC_ERROR_NO_MEMORY;

    *out_address = virt_addr;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _framebuffer_command_op(
    rsrc_t *resource,
    void *handle_state,
    uint64_t command_id,
    const void *in,
    uint64_t in_len,
    void *out,
    uint64_t out_len,
    uint64_t *out_bytes_written)
{
    if (resource == NULL || resource->type_state == NULL
        || command_id != FRAMEBUFFER_DEVICE_COMMAND_GET_INFO || out == NULL
        || out_len < sizeof(framebuffer_device_info_t)) {
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    (void) handle_state;
    (void) in;
    (void) in_len;

    _framebuffer_device_state_t *state = (_framebuffer_device_state_t *) resource->type_state;
    framebuffer_device_info_t info = {
        .width = state->framebuffer.width,
        .height = state->framebuffer.height,
        .pitch = state->framebuffer.pitch,
        .bpp = state->framebuffer.bpp,
        .memory_model = state->framebuffer.memory_model,
        .red_mask_size = state->framebuffer.red_mask_size,
        .red_mask_shift = state->framebuffer.red_mask_shift,
        .green_mask_size = state->framebuffer.green_mask_size,
        .green_mask_shift = state->framebuffer.green_mask_shift,
        .blue_mask_size = state->framebuffer.blue_mask_size,
        .blue_mask_shift = state->framebuffer.blue_mask_shift,
        .size = state->size,
    };

    memcpy(out, &info, sizeof(info));
    if (out_bytes_written != NULL)
        *out_bytes_written = sizeof(info);
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _framebuffer_device_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = _framebuffer_seek_op,
    .list = NULL,
    .read = _framebuffer_read_op,
    .write = _framebuffer_write_op,
    .mmap = _framebuffer_mmap_op,
    .poll = NULL,
    .remove = NULL,
    .control = _framebuffer_command_op,
};

static bool _register_framebuffer_device(uint8_t index)
{
    if (index >= _framebuffer_count)
        return false;

    char path[RSRC_PATH_MAX_LEN];
    if (_build_framebuffer_device_path(path, sizeof(path), index) != 0)
        return false;

    framebuffer_t *framebuffer = &_framebuffers[index];
    _framebuffer_device_states[index] = (_framebuffer_device_state_t){
        .framebuffer = *framebuffer,
        .phys_addr = (uintptr_t) vmm_get_lhdm_addr(framebuffer->address),
        .size = framebuffer->pitch * framebuffer->height,
    };

    return device_register(path, &_framebuffer_device_ops, &_framebuffer_device_states[index])
           != NULL;
}

void setup_framebuffer(framebuffer_t fb)
{
    if (_framebuffer_count >= MAX_FRAMEBUFFERS)
        return;

    uint8_t index = _framebuffer_count++;
    _framebuffers[index] = fb;
    if (_framebuffer_devices_ready)
        _register_framebuffer_device(index);
}

framebuffer_t *get_framebuffer(uint8_t index)
{
    return index >= _framebuffer_count ? NULL : &_framebuffers[index];
}

uint8_t get_framebuffer_count(void)
{
    return _framebuffer_count;
}

bool framebuffer_devices_init(void)
{
    if (_framebuffer_devices_ready)
        return true;
    if (!device_domain_init())
        return false;

    for (uint8_t i = 0; i < _framebuffer_count; i++) {
        if (!_register_framebuffer_device(i))
            return false;
    }

    _framebuffer_devices_ready = true;
    return true;
}
