;
; Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
; SPDX-License-Identifier: GPL-3.0
;

global sse_init
sse_init:
    ; https://wiki.osdev.org/SSE#Adding_support
    mov eax, cr0
    and ax, 0xFFFB
    or  ax, 0x2
    mov cr0, eax
    mov eax, cr4
    or  ax, 3<<9
    mov cr4, eax
    ret

global sse_save
sse_save:
    mov eax, [esp + 4]
    fxsave [eax]
    ret

global sse_restore
sse_restore:
    mov eax, [esp + 4]
    fxrstor [eax]
    ret
