/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void interrupts_eoi(uint8_t isr);
void interrupts_set_irq_mask(uint8_t irq, bool mask);
void interrupts_disable();
void interrupts_enable();
