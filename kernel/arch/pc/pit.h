/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

#include <stdint.h>

uint16_t pit_get_count();
void pit_set_reload(uint16_t);
void pit_set_frequency(uint16_t);
