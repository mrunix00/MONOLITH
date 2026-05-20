/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

void apic_init(void *rsdp);
void apic_eoi();
void apic_set_irq_mask(uint8_t irq, bool masked);
bool apic_is_initialized();
