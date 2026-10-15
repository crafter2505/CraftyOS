BITS 64

global syscall_entry
extern syscall_handler

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

    mov rdi, [rsp + 112]
    mov rsi, rsp

    sti

    call syscall_handler

    cli
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

    mov rax, rcx
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
    mov rax, [rsp]
    add rsp, 8

    pop rcx
    pop r11
    pop rsp

    swapgs
    o64 sysret