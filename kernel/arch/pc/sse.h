/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#pragma once

/*
 * Enable SSE instructions.
 * https://wiki.osdev.org/SSE
 */
void sse_init();

/*
 * Save the current SSE state.
 * https://wiki.osdev.org/SSE#FXSAVE_and_FXRSTOR
 */
void sse_save(void *ctx);

/*
 * Restore the saved SSE state.
 * https://wiki.osdev.org/SSE#FXSAVE_and_FXRSTOR
 */
void sse_restore(void *ctx);
