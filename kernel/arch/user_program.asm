BITS 64

section .user_program
global user_program_start
global user_program_end
global user_program_msg

user_program_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel user_program_msg]
    mov rdx, user_program_msg_len
    syscall

    ; exit(0)
    mov rax, 0
    xor rdi, rdi
    syscall

    ud2

user_program_msg:
    db "hello from ring 3", 10
user_program_msg_end:

user_program_msg_len equ user_program_msg_end - user_program_msg

user_program_end: