BITS 64

global syscall_entry
extern syscall_handler

%define SR_R15      0
%define SR_R14      8
%define SR_R13      16
%define SR_R12      24
%define SR_R11      32
%define SR_R10      40
%define SR_R9       48
%define SR_R8       56
%define SR_RBP      64
%define SR_RDI      72
%define SR_RSI      80
%define SR_RDX      88
%define SR_RCX      96
%define SR_RBX      104
%define SR_RAX      112
%define SR_RIP      120
%define SR_RFLAGS   128
%define SR_RSP      136

section .text

syscall_entry:
    swapgs
    mov [gs:8], rsp
    mov rsp, [gs:0]

    push qword [gs:8]
    push r11
    push rcx

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + SR_RAX]
    mov rsi, rsp
    sti
    call syscall_handler
    cli

    mov rdx, [rsp + SR_RIP]
    mov rax, rdx
    sar rax, 47
    cmp rax, 0
    je .canonical
    cmp rax, -1
    je .canonical
.bad_rip:
    cli
    hlt
    jmp .bad_rip

.canonical:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx

    mov rax, [rsp + 0]
    mov rcx, [rsp + 8]
    mov r11, [rsp + 16]
    add rsp, 24
    pop rsp

    swapgs
    o64 sysret