section .init
global _init:function
_init:
%ifidn __OUTPUT_FORMAT__, elf32
    push ebp
    mov ebp, esp
%else
    push rbp
    mov rbp, rsp
%endif

section .fini
global _fini:function
_fini:
%ifidn __OUTPUT_FORMAT__, elf32
    push ebp
    mov ebp, esp
%else
    push rbp
    mov rbp, rsp
%endif
