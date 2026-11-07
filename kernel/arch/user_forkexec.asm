BITS 64

section .user_fork_exec
global user_fork_exec_start
global user_fork_exec_end

user_fork_exec_start:
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
    ; exec("/mnt/efi/boot/hello.elf")
    mov rax, 12
    lea rdi, [rel path]
    syscall

    ; exec returned — it failed. Exit with 1.
    mov rdi, 1
    mov rax, 0
    syscall
    ud2

path:
    db "/mnt/efi/boot/hello_test.elf", 0

user_fork_exec_end: