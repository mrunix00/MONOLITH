;
; IA-32 ISR/IRQ stubs.
;

%macro PUSHALL 0
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
%endmacro

%macro POPALL 0
    add esp, 32
    pop esi
    pop edi
    pop ebp
    pop edx
    pop ecx
    pop ebx
    pop eax
%endmacro

%macro ISR_NOERRCODE 1
    global _isr%1
    _isr%1:
        push dword 0
        push dword %1
        push dword 0
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    global _isr%1
    _isr%1:
        push dword %1
        push dword 0
        jmp isr_common_stub
%endmacro

%macro IRQ 2
    global _irq%1
    _irq%1:
        push dword 0
        push dword %2
        push dword 0
        jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern isr_handler
isr_common_stub:
    PUSHALL
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp
    call isr_handler
    add esp, 4
    mov ax, [esp + 76]
    and ax, 3
    cmp ax, 3
    jne .isr_kernel_data_segments
    mov ax, 0x23
    jmp .isr_load_data_segments
.isr_kernel_data_segments:
    mov ax, 0x10
.isr_load_data_segments:
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    POPALL
    add esp, 12
    iretd

extern irq_handler
irq_common_stub:
    PUSHALL
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp
    call irq_handler
    add esp, 4
    mov ax, [esp + 76]
    and ax, 3
    cmp ax, 3
    jne .irq_kernel_data_segments
    mov ax, 0x23
    jmp .irq_load_data_segments
.irq_kernel_data_segments:
    mov ax, 0x10
.irq_load_data_segments:
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    POPALL
    add esp, 12
    iretd
