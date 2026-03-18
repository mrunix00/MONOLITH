/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

/* Status register bits */
#define PS2_STATUS_OBF 0x01 /* Output buffer full (data ready to read) */
#define PS2_STATUS_IBF 0x02 /* Input buffer full (controller busy)      */
#define PS2_STATUS_AUX 0x20 /* Output buffer data is from aux (mouse)   */

/* Controller commands */
#define PS2_CMD_READ_CCB 0x20
#define PS2_CMD_WRITE_CCB 0x60
#define PS2_CMD_DISABLE_KB 0xAD
#define PS2_CMD_ENABLE_KB 0xAE
#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_WRITE_AUX 0xD4

/* PS/2 I/O ports */
#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

/* CCB bits */
#define PS2_CCB_KB_IRQ_ENABLE 0x01
#define PS2_CCB_AUX_IRQ_ENABLE 0x02
#define PS2_CCB_KB_CLOCK_OFF 0x10
#define PS2_CCB_AUX_CLOCK_OFF 0x20

void ps2_wait_ibf_clear();
void ps2_wait_obf_set();
void ps2_wait_obf_clear();

void ps2_write_command(uint8_t cmd);
void ps2_write(uint8_t value);
uint8_t ps2_read();

uint8_t ps2_read_config();
void ps2_write_config(uint8_t ccb);
