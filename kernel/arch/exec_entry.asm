BITS 64

global exec_enter_user_asm

%define EF_RAX     0
%define EF_RBX     8
%define EF_RCX     16
%define EF_RDX     24
%define EF_RSI     32
%define EF_RDI     40
%define EF_RBP     48
%define EF_R8      56
%define EF_R9      64
%define EF_R10     72
%define EF_R11     80
%define EF_R12     88
%define EF_R13     96
%define EF_R14     104
%define EF_R15     112
%define EF_RIP     120
%define EF_RFLAGS  128
%define EF_RSP     136

%define USER_CS    0x2B
%define USER_SS    0x23

section .text

exec_enter_user_asm:
    mov r13, rdi

    push qword USER_SS
    push qword [r13 + EF_RSP]
    push qword [r13 + EF_RFLAGS]
    push qword USER_CS
    push qword [r13 + EF_RIP]

    mov rax, [r13 + EF_RAX]
    mov rbx, [r13 + EF_RBX]
    mov rcx, [r13 + EF_RCX]
    mov rdx, [r13 + EF_RDX]
    mov rsi, [r13 + EF_RSI]
    mov rdi, [r13 + EF_RDI]
    mov rbp, [r13 + EF_RBP]
    mov r8,  [r13 + EF_R8]
    mov r9,  [r13 + EF_R9]
    mov r10, [r13 + EF_R10]
    mov r11, [r13 + EF_R11]
    mov r12, [r13 + EF_R12]
    mov r14, [r13 + EF_R14]
    mov r15, [r13 + EF_R15]
    mov r13, [r13 + EF_R13]

    swapgs
    iretq  