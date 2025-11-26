/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

void asm_invlpg(void *virt_addr);
uintptr_t asm_read_cr2();
uintptr_t asm_read_cr3();
void asm_write_cr3(uintptr_t value);
void asm_hlt();
void asm_outb(unsigned char value, unsigned short int port);
unsigned char asm_inb(unsigned short int port);
uintptr_t asm_read_rsp();
