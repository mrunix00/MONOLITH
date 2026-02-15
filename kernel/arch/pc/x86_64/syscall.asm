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
extern sys_exit
extern sys_request_fb
extern sys_poll_input_event
extern sys_file_open
extern sys_file_close
extern sys_file_create
extern sys_file_remove
extern sys_file_read
extern sys_file_write
extern sys_file_seek
extern sys_file_getdents
extern sys_file_getstats
extern sys_file_tell
extern sys_getdrives
extern sys_sleep
extern sys_get_ticks
extern sys_alloc_pages
extern sys_spawn_task
extern sys_test
extern sys_ipc_new
extern sys_ipc_request_connection
extern sys_ipc_wait_connection
extern sys_ipc_accept_connection
extern sys_ipc_reject_connection
extern sys_ipc_send_to
extern sys_ipc_send
extern sys_ipc_receive
extern sys_ipc_disconnect

section .rodata
syscall_table:
    dq sys_exit
    dq sys_request_fb
    dq sys_poll_input_event
    dq sys_file_open
    dq sys_file_close
    dq sys_file_create
    dq sys_file_remove
    dq sys_file_read
    dq sys_file_write
    dq sys_file_seek
    dq sys_file_getdents
    dq sys_file_getstats
    dq sys_file_tell
    dq sys_getdrives
    dq sys_sleep
    dq sys_get_ticks
    dq sys_alloc_pages
    dq sys_spawn_task
    dq sys_test
    dq sys_ipc_new
    dq sys_ipc_request_connection
    dq sys_ipc_wait_connection
    dq sys_ipc_accept_connection
    dq sys_ipc_reject_connection
    dq sys_ipc_send
    dq sys_ipc_receive
    dq sys_ipc_disconnect
    dq sys_ipc_send_to
syscall_table_end:

section .text
global _syscall_handler
_syscall_handler:
    cmp rax, (syscall_table_end-syscall_table) / 8
    jge .invalid
    PUSHALL
    xor rbp, rbp
    call [syscall_table + rax * 8]
    POPALL
    iretq
.invalid
    mov rax, -1
    iretq
