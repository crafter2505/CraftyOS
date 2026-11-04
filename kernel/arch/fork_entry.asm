BITS 64

global fork_enter_user_asm

%define FF_RAX     0
%define FF_RBX     8
%define FF_RCX     16
%define FF_RDX     24
%define FF_RSI     32
%define FF_RDI     40
%define FF_RBP     48
%define FF_R8      56
%define FF_R9      64
%define FF_R10     72
%define FF_R11     80
%define FF_R12     88
%define FF_R13     96
%define FF_R14     104
%define FF_R15     112
%define FF_RIP     120
%define FF_RFLAGS  128
%define FF_RSP     136

%define USER_CS    0x2B
%define USER_SS    0x23

section .text

fork_enter_user_asm:
    mov r13, rdi

    push qword USER_SS
    push qword [r13 + FF_RSP]
    push qword [r13 + FF_RFLAGS]
    push qword USER_CS
    push qword [r13 + FF_RIP]

    mov rax, [r13 + FF_RAX]
    mov rbx, [r13 + FF_RBX]
    mov rcx, [r13 + FF_RCX]
    mov rdx, [r13 + FF_RDX]
    mov rsi, [r13 + FF_RSI]
    mov rdi, [r13 + FF_RDI]
    mov rbp, [r13 + FF_RBP]
    mov r8,  [r13 + FF_R8]
    mov r9,  [r13 + FF_R9]
    mov r10, [r13 + FF_R10]
    mov r11, [r13 + FF_R11]
    mov r12, [r13 + FF_R12]
    mov r14, [r13 + FF_R14]
    mov r15, [r13 + FF_R15]
    mov r13, [r13 + FF_R13]

    swapgs
    iretq