;
; Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
; SPDX-License-Identifier: GPL-3.0
;

%macro PUSHALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POPALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

section .text

global _task_switch_gate_stub
extern _task_switch_gate

_task_switch_gate_stub:
    push 0                ; error code placeholder
    push 0x30             ; interrupt vector
    push fs               ; core placeholder
    PUSHALL
    cld
    mov rdi, rsp
    xor rbp, rbp
    call _task_switch_gate
    POPALL
    add rsp, 24
    iretq
