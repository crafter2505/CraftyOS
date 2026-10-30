BITS 64

section .user_program_file
global user_file_program_start
global user_file_program_end

user_file_program_start:
    ; fd = open("/mnt/efi/boot/kernel.elf", O_RDONLY)
    mov rax, 7              ; SYS_open
    lea rdi, [rel path]
    xor rsi, rsi            ; flags = 0 (O_RDONLY)
    syscall

    ; If fd < 0, exit(1)
    test rax, rax
    js .fail

    mov r12, rax            ; save fd

    ; We'll test the fd is valid by closing it
    mov rax, 8              ; SYS_close
    mov rdi, r12
    syscall

    ; If close returned 0, exit(0)
    test rax, rax
    js .fail

    ; Success
    mov rax, 0              ; SYS_exit
    xor rdi, rdi            ; status = 0
    syscall

.fail:
    mov rax, 0              ; SYS_exit
    mov rdi, 1              ; status = 1
    syscall

    ud2

path:
    db "/mnt/efi/boot/kernel.elf", 0

user_file_program_end: