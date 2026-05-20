/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void pic_init();
void pic_disable();
void pic_eoi(uint8_t isr);
void pic_set_irq_mask(uint8_t irq, bool mask);
bool pic_is_enabled();
