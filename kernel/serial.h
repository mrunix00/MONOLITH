/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Serial port addresses.
 * https://wiki.osdev.org/Serial_Ports#Port_Addresses
 */
typedef enum {
    SERIAL_COM1 = 0x3F8,
    SERIAL_COM2 = 0x2F8,
    SERIAL_COM3 = 0x3E8,
    SERIAL_COM4 = 0x2E8,
    SERIAL_COM5 = 0x2E0,
    SERIAL_COM6 = 0x360,
    SERIAL_COM7 = 0x3E0,
    SERIAL_COM8 = 0x2A0,
} serial_port_t;

/*
 * Initialize the selected serial port.
 * Returns true if the initialization was successful, false otherwise.
 */
bool init_serial(serial_port_t port);

/*
 * Write a character to the selected serial port.
 * Returns true if the character was written successfully, false otherwise.
 */
void write_serial(serial_port_t port, char c);

/*
 * Write a string to the selected serial port.
 * Returns the number of bytes written to the port.
 */
void write_bytes(serial_port_t port, const char *buffer, size_t size);

/*
 * Writes a null-terminated string to the selected serial port.
 */
void write_string(serial_port_t port, const char *str);

/*
 * Read a character from the selected serial port.
 * Returns the character read from the port.
 */
char read_serial(serial_port_t port);

/*
 * Read a string from the selected serial port.
 * Returns the number of bytes read from the port.
 */
size_t read_bytes(serial_port_t port, char *buffer, size_t size);
