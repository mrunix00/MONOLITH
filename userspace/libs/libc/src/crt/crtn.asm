section .init
%ifidn __OUTPUT_FORMAT__, elf32
    pop ebp
%else
    pop rbp
%endif
    ret

section .fini
%ifidn __OUTPUT_FORMAT__, elf32
    pop ebp
%else
    pop rbp
%endif
    ret
