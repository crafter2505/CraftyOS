BITS 64

section .user_program
global _user_fault_program_start
global _user_fault_program_end

_user_fault_program_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg1]
    mov rdx, msg1_len
    syscall

    mov rax, 0xDEADBEEF0000
    mov qword [rax], 42

    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg2]
    mov rdx, msg2_len
    syscall

    mov rax, 0
    xor rdi, rdi
    syscall

msg1: db "user program started (about to fault)", 10
msg1_len equ $ - msg1

msg2: db "user program survived the fault (BUG!!!!!!)", 10
msg2_len equ $ - msg2

_user_fault_program_end: