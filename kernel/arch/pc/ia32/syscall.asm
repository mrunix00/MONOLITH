section .text
global _syscall_handler
extern syscall_dispatch
_syscall_handler:
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    mov edi, eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esi
    push edx
    push ecx
    push ebx
    push edi
    call syscall_dispatch
    add esp, 20
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    push eax
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    pop eax
    iretd
