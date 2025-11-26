;
; Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
; SPDX-License-Identifier: GPL-3.0
;

global asm_invlpg
asm_invlpg:
    invlpg [rdi]
    ret

global asm_read_cr2
asm_read_cr2:
    mov rax, cr2
    ret

global asm_read_cr3
asm_read_cr3:
    mov rax, cr3
    ret

global asm_write_cr3
asm_write_cr3:
    mov cr3, rdi
    ret

global asm_hlt
asm_hlt:
    hlt
    ret

global asm_outb
asm_outb:
    mov al, dil
    mov dx, si
    out dx, al
    ret

global asm_inb
asm_inb:
    mov dx, di
    in al, dx
    ret

global asm_read_rsp
asm_read_rsp:
    mov rax, rsp
    ret
