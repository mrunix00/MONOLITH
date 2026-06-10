;
; IA-32 Multiboot2 entry point.
;

MB2_MAGIC equ 0xE85250D6
MB2_ARCH_I386 equ 0
MB2_HEADER_LENGTH equ mb2_header_end - mb2_header_start
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH_I386 + MB2_HEADER_LENGTH)

section .multiboot2 align=8
mb2_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd MB2_HEADER_LENGTH
    dd MB2_CHECKSUM
    ; framebuffer tag: type=5, flags=0, size=20, width=0, height=0, depth=32
    dw 5
    dw 0
    dd 20
    dd 0
    dd 0
    dd 32
    align 8
    ; end tag: type=0, flags=0, size=8
    dw 0
    dw 0
    dd 8
mb2_header_end:

section .bss align=16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top
    push ebx
    push eax
    call kmain
.hang:
    cli
    hlt
    jmp .hang
