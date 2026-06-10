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
    add rsp, 8      ; skip rax - preserve syscall return value
%endmacro

section .text
extern syscall_kernel_stack_top
extern syscall_dispatch

section .bss
syscall_saved_user_rsp:
    resq 1

section .text
global _syscall_handler
_syscall_handler:
    mov [rel syscall_saved_user_rsp], rsp
    mov rsp, [rel syscall_kernel_stack_top]

    ; Build an iretq frame. syscall saves the userspace RIP in RCX and
    ; RFLAGS in R11, but does not save or switch RSP for us.
    push qword 0x23
    push qword [rel syscall_saved_user_rsp]
    push r11
    push qword 0x1B
    push rcx

    PUSHALL
    xor rbp, rbp
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call syscall_dispatch
    POPALL
    iretq
