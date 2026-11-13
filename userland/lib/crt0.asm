BITS 64

global _start
extern main
extern _exit

section .text.start

_start:
    xor rbp, rbp
    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    and rsp, -16
    call main
    mov rdi, rax
    call _exit
    ud2
