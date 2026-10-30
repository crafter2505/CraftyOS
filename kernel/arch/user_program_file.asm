BITS 64

section .user_program_file
global user_file_program_start
global user_file_program_end

user_file_program_start:
    ; fd = open("/mnt/efi/boot/kernel.elf", O_RDONLY)
    mov rax, 7                  ; SYS_open
    lea rdi, [rel path]
    xor rsi, rsi                ; flags = 0
    syscall
    test rax, rax
    js .fail
    mov r12, rax                ; r12 = fd

    ; read(fd, buffer, 4)
    mov rax, 9                  ; SYS_read
    mov rdi, r12
    lea rsi, [rel buffer]
    mov rdx, 4
    syscall
    cmp rax, 4
    jne .fail_close

    ; Verify ELF magic { 0x7F, 'E', 'L', 'F' }
    lea rsi, [rel buffer]
    cmp byte [rsi + 0], 0x7F
    jne .fail_close
    cmp byte [rsi + 1], 'E'
    jne .fail_close
    cmp byte [rsi + 2], 'L'
    jne .fail_close
    cmp byte [rsi + 3], 'F'
    jne .fail_close

    ; lseek(fd, 0, SEEK_SET) -- rewind (test lseek)
    mov rax, 10                 ; SYS_lseek
    mov rdi, r12
    xor rsi, rsi                ; offset = 0
    xor rdx, rdx                ; whence = SEEK_SET
    syscall
    test rax, rax
    js .fail_close

    ; close(fd)
    mov rax, 8                  ; SYS_close
    mov rdi, r12
    syscall

    ; exit(0) -- success
    mov rax, 0                  ; SYS_exit
    xor rdi, rdi
    syscall
    ud2

.fail_close:
    mov rax, 8                  ; SYS_close
    mov rdi, r12
    syscall
.fail:
    mov rax, 0                  ; SYS_exit
    mov rdi, 1                  ; status = 1
    syscall
    ud2

path:   db "/mnt/efi/boot/kernel.elf", 0
buffer: times 8 db 0

user_file_program_end: