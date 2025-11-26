;
; Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
; SPDX-License-Identifier: GPL-3.0
;

global sse_init
sse_init:
    ; https://wiki.osdev.org/SSE#Adding_support
    mov rax, cr0
    and ax, 0xFFFB
    or  ax, 0x2
    mov cr0, rax
    mov rax, cr4
    or  ax, 3<<9
    mov cr4, rax
    ret
