BITS 64

global enter_userspace
global userspace_exit_to_kernel

extern thread_current

%define KC_RSP      0
%define KC_RBP      8
%define KC_RBX      16
%define KC_R12      24
%define KC_R13      32
%define KC_R14      40
%define KC_R15      48
%define KC_RIP      56

%define THREAD_SYSCALL_STACK      48
%define THREAD_SYSCALL_STACK_SIZE 56
%define THREAD_USER_CONT          208

%define USER_CS     0x2B
%define USER_SS     0x23
%define USER_RFLAGS 0x202

section .text

enter_userspace:
    test rdi, rdi
    jz .bad_args
    lea rax, [rdi + THREAD_USER_CONT]
    mov [rax + KC_RSP], rsp
    lea rcx, [rel .resume]
    mov [rax + KC_RIP], rcx
    mov [rax + KC_RBP], rbp
    mov [rax + KC_RBX], rbx
    mov [rax + KC_R12], r12
    mov [rax + KC_R13], r13
    mov [rax + KC_R14], r14
    mov [rax + KC_R15], r15
    mov rcx, [rdi + THREAD_SYSCALL_STACK]
    test rcx, rcx
    jz .bad_args
    mov rax, [rdi + THREAD_SYSCALL_STACK_SIZE]
    test rax, rax
    jz .bad_args
    add rcx, rax
    mov rsp, rcx
    push qword USER_SS
    push rdx
    push qword USER_RFLAGS
    push qword USER_CS
    push rsi

    swapgs
    iretq

.bad_args:
    mov rax, -1
    ret

.resume:
    xor eax, eax
    ret

userspace_exit_to_kernel:
    cli
    call thread_current
    test rax, rax
    jz .halt

    mov r8, rax
    add r8, THREAD_USER_CONT

    mov rbp, [r8 + KC_RBP]
    mov rbx, [r8 + KC_RBX]
    mov r12, [r8 + KC_R12]
    mov r13, [r8 + KC_R13]
    mov r14, [r8 + KC_R14]
    mov r15, [r8 + KC_R15]

    mov rsp, [r8 + KC_RSP]
    jmp qword [r8 + KC_RIP]

.halt:
    cli
    hlt
    jmp .halt