BITS 64

section .user_fork_program
global user_fork_program_start
global user_fork_program_end

user_fork_program_start:
    mov rax, 11
    syscall

    test rax, rax
    jz .child

.parent:
    mov rdi, rax
    mov rax, 5
    syscall

    mov rax, 0
    xor rdi, rdi
    syscall
    ud2

.child:
    mov rax, 0
    mov rdi, 42
    syscall
    ud2

user_fork_program_end: