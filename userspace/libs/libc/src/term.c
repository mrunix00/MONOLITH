/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <resource.h>
#include <term.h>
#include <unistd.h>

static int _term_read_exact(rsrc_handle_t rd, void *buffer, uint32_t length)
{
    uint8_t *cursor = (uint8_t *) buffer;
    uint32_t total = 0;

    while (total < length) {
        int bytes_read = read(rd, cursor + total, length - total);
        if (bytes_read < 0)
            return -1;
        if (bytes_read == 0) {
            if (total == 0)
                return 0;

            rsrc_poll_t poll = {.handle = rd, .events = RSRC_POLL_READ};
            rsmgr_poll(&poll, 1);
            continue;
        }
        total += (uint32_t) bytes_read;
    }

    return 1;
}

static int _term_write_all(rsrc_handle_t rd, const void *buffer, uint32_t length)
{
    const uint8_t *cursor = (const uint8_t *) buffer;
    uint32_t total = 0;

    while (total < length) {
        int bytes_written = write(rd, cursor + total, length - total);
        if (bytes_written < 0)
            return -1;
        if (bytes_written == 0) {
            rsrc_poll_t poll = {.handle = rd, .events = RSRC_POLL_WRITE};
            rsmgr_poll(&poll, 1);
            continue;
        }
        total += (uint32_t) bytes_written;
    }

    return 0;
}

static int _term_drain_exact(rsrc_handle_t rd, uint32_t length)
{
    char discard[128];

    while (length > 0) {
        uint32_t chunk = length > sizeof(discard) ? sizeof(discard) : length;
        int result = _term_read_exact(rd, discard, chunk);
        if (result <= 0)
            return -1;
        length -= chunk;
    }

    return 0;
}

int term_write_command(
    rsrc_handle_t rd, term_command_type_t type, const void *payload, uint32_t length)
{
    term_command_t command = {.type = type, .length = length};

    if (length > TERM_MAX_PAYLOAD || (length > 0 && payload == NULL))
        return -1;
    if (_term_write_all(rd, &command, sizeof(command)) != 0)
        return -1;
    if (length == 0)
        return 0;

    return _term_write_all(rd, payload, length);
}

int term_write_event(rsrc_handle_t rd, term_event_type_t type, const void *payload, uint32_t length)
{
    term_event_t event = {.type = type, .length = length};
    if (length > TERM_MAX_PAYLOAD || (length > 0 && payload == NULL)
        || _term_write_all(rd, &event, sizeof(event)) != 0)
        return -1;
    return length == 0 ? 0 : _term_write_all(rd, payload, length);
}

int term_read_command(rsrc_handle_t rd, term_command_t *cmd, void *payload, uint32_t length)
{
    int result;

    if (cmd == NULL)
        return -1;

    result = _term_read_exact(rd, cmd, sizeof(*cmd));
    if (result <= 0)
        return result;

    if (cmd->length > length || cmd->length > TERM_MAX_PAYLOAD) {
        _term_drain_exact(rd, cmd->length);
        return -1;
    }

    if (cmd->length == 0)
        return 1;

    return payload == NULL ? -1 : _term_read_exact(rd, payload, cmd->length) == 1 ? 1 : -1;
}

int term_read_event(rsrc_handle_t rd, term_event_t *event, void *payload, uint32_t length)
{
    int result;

    if (event == NULL)
        return -1;

    result = _term_read_exact(rd, event, sizeof(*event));
    if (result <= 0)
        return result;

    if (event->length > length || event->length > TERM_MAX_PAYLOAD) {
        _term_drain_exact(rd, event->length);
        return -1;
    }

    if (event->length == 0)
        return 1;
    if (payload == NULL)
        return -1;

    return _term_read_exact(rd, payload, event->length) == 1 ? 1 : -1;
}
