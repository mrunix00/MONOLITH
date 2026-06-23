/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/devices/device_domain.h>
#include <kernel/devices/serial.h>
#include <stdint.h>

bool init_serial(serial_port_t port)
{
    /* https://wiki.osdev.org/Serial_Ports#Initialization */
    asm_outb(0x00, port + 1); /* Disable all interrupts */
    asm_outb(0x80, port + 3); /* Enable DLAB (set baud rate divisor) */
    asm_outb(0x03, port + 0); /* Set divisor to 3 (lo byte) 38400 baud */
    asm_outb(0x00, port + 1); /* (hi byte) */
    asm_outb(0x03, port + 3); /* 8 bits, no parity, one stop bit */
    asm_outb(0xC7, port + 2); /* Enable FIFO */
    asm_outb(0x0B, port + 4); /* IRQs enabled, RTS/DSR set */
    asm_outb(0x1E, port + 4); /* Set in loopback mode, test the serial chip */
    asm_outb(0xAE, port + 0); /* Test serial chip */

    /* Check if serial is faulty (i.e: not same byte as sent) */
    if (asm_inb(port + 0) != 0xAE) {
        return 0;
    }

    /* If serial is not faulty set it in normal operation mode
	(not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled) */
    asm_outb(0x0F, port + 4);
    return 1;
}

char read_serial(serial_port_t port)
{
    /* https://wiki.osdev.org/Serial_Ports#Receiving_data */
    while ((asm_inb(port + 5) & 1) == 0)
        ;
    return asm_inb(port);
}

void write_serial(serial_port_t port, char c)
{
    /* https://wiki.osdev.org/Serial_Ports#Sending_data */
    while ((asm_inb(port + 5) & 0x20) == 0)
        ;
    asm_outb(c, port);
}

void write_bytes(serial_port_t port, const char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
        write_serial(port, buffer[i]);
}

void write_string(serial_port_t port, const char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
        write_serial(port, str[i]);
}

size_t read_bytes(serial_port_t port, char *buffer, size_t size)
{
    size_t i = 0;
    for (; i < size; i++)
        buffer[i] = read_serial(port);
    return i;
}

static rsrc_status_t _serial_dev_read(
    rsrc_t *resource, void *handle_state, void *buffer, uint64_t buffer_len, uint64_t *out_bytes_read)
{
    (void) handle_state;

    if (buffer == NULL || buffer_len == 0 || out_bytes_read == NULL)
        return RSRC_ERROR_INVALID_ARGUMENT;

    serial_port_t port = (serial_port_t) (uintptr_t) resource->type_state;
    uint64_t count = read_bytes(port, buffer, buffer_len);

    if (count == 0)
        return RSRC_ERROR_WOULD_BLOCK;

    *out_bytes_read = count;
    return RSRC_STATUS_OK;
}

static rsrc_status_t _serial_dev_write(
    rsrc_t *resource,
    void *handle_state,
    const void *buffer,
    uint64_t buffer_len,
    uint64_t *out_bytes_written)
{
    (void) handle_state;

    if (buffer == NULL || buffer_len == 0) {
        if (out_bytes_written != NULL)
            *out_bytes_written = 0;
        return RSRC_ERROR_INVALID_ARGUMENT;
    }

    serial_port_t port = (serial_port_t) (uintptr_t) resource->type_state;
    write_bytes(port, buffer, buffer_len);

    if (out_bytes_written != NULL)
        *out_bytes_written = buffer_len;
    return RSRC_STATUS_OK;
}

static const rsrc_ops_t _serial_ops = {
    .open = NULL,
    .lookup = NULL,
    .dup_handle = NULL,
    .close_handle = NULL,
    .destroy = NULL,
    .describe = NULL,
    .seek = NULL,
    .list = NULL,
    .read = _serial_dev_read,
    .write = _serial_dev_write,
    .mmap = NULL,
    .poll = NULL,
    .remove = NULL,
    .control = NULL,
};

void serial_devices_init(void)
{
    static serial_port_t _serial_ports[] = {
        SERIAL_COM1,
        SERIAL_COM2,
        SERIAL_COM3,
        SERIAL_COM4,
        SERIAL_COM5,
        SERIAL_COM6,
        SERIAL_COM7,
        SERIAL_COM8,
    };
    for (uint8_t i = 0; i < sizeof(_serial_ports) / sizeof(_serial_ports[0]); i++) {
        char path[16] = "/serialX";
        path[7] = '0' + i;
        device_register(path, &_serial_ops, (void *) (uintptr_t) _serial_ports[i]);
    }
}
