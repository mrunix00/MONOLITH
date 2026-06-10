section .text
global _task_switch_gate_stub
extern _task_switch_gate
_task_switch_gate_stub:
    push dword 0
    push dword 0x30
    push dword 0
    push eax
    push ebx
    push ecx
    push edx
    push ebp
    push edi
    push esi
    push dword 0
    push dword 0
    push dword 0
    push dword 0
    push dword 0
    push dword 0
    push dword 0
    push dword 0
    cld
    push esp
    call _task_switch_gate
    add esp, 4
    mov ax, [esp + 76]
    and ax, 3
    cmp ax, 3
    jne .kernel_data_segments
    mov ax, 0x23
    jmp .load_data_segments
.kernel_data_segments:
    mov ax, 0x10
.load_data_segments:
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    add esp, 32
    pop esi
    pop edi
    pop ebp
    pop edx
    pop ecx
    pop ebx
    pop eax
    add esp, 12
    iretd
