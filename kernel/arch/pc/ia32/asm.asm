;
; IA-32 low-level helpers.
;

global asm_invlpg
asm_invlpg:
    mov eax, [esp + 4]
    invlpg [eax]
    ret

global asm_read_cr2
asm_read_cr2:
    mov eax, cr2
    ret

global asm_read_cr3
asm_read_cr3:
    mov eax, cr3
    ret

global asm_write_cr3
asm_write_cr3:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

global asm_hlt
asm_hlt:
    hlt
    ret

global asm_outb
asm_outb:
    mov al, [esp + 4]
    mov dx, [esp + 8]
    out dx, al
    ret

global asm_inb
asm_inb:
    mov dx, [esp + 4]
    xor eax, eax
    in al, dx
    ret

global asm_read_rsp
asm_read_rsp:
    mov eax, esp
    ret

global asm_pause
asm_pause:
    pause
    ret

global asm_cli
asm_cli:
    cli
    ret

global asm_sti
asm_sti:
    sti
    ret

global asm_read_msr
asm_read_msr:
    mov ecx, [esp + 4]
    rdmsr
    ret

global asm_write_msr
asm_write_msr:
    mov ecx, [esp + 4]
    mov eax, [esp + 8]
    mov edx, [esp + 12]
    wrmsr
    ret

global asm_cpuid
asm_cpuid:
    push ebx
    push esi
    push edi
    sub esp, 16
    mov esi, [esp + 32]
    mov [esp + 0], esi
    mov esi, [esp + 36]
    mov [esp + 4], esi
    mov esi, [esp + 40]
    mov [esp + 8], esi
    mov esi, [esp + 44]
    mov [esp + 12], esi
    mov eax, 1
    cpuid
    mov esi, [esp + 0]
    mov [esi], eax
    mov esi, [esp + 4]
    mov [esi], ebx
    mov esi, [esp + 8]
    mov [esi], ecx
    mov esi, [esp + 12]
    mov [esi], edx
    add esp, 16
    pop edi
    pop esi
    pop ebx
    ret
