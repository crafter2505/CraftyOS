BITS 64

global syscall_entry
extern syscall_handler

; Offsets matching struct syscall_regs in kernel/arch/syscall_regs.h.
%define SR_R15      0
%define SR_R14      8
%define SR_R13      16
%define SR_R12      24
%define SR_R11      32
%define SR_R10      40
%define SR_R9       48
%define SR_R8       56
%define SR_RBP      64
%define SR_RDI      72
%define SR_RSI      80
%define SR_RDX      88
%define SR_RCX      96
%define SR_RBX      104
%define SR_RAX      112
%define SR_RIP      120
%define SR_RFLAGS   128
%define SR_RSP      136

section .text

syscall_entry:
    ; SYSCALL has already:
    ;   - loaded CS/SS from IA32_STAR
    ;   - saved user RIP in RCX
    ;   - saved user RFLAGS in R11
    ;   - masked RFLAGS by IA32_FMASK
    ;   - NOT saved user RSP anywhere (still in RSP if the kernel
    ;     stack were swapped by other means; here we save it manually)
    swapgs
    mov [gs:8], rsp                 ; stash user rsp in per-CPU scratch
    mov rsp, [gs:0]                 ; switch to kernel rsp

    ; Build struct syscall_regs on the kernel stack.
    ; Push in reverse order so that after all pushes, [rsp+0] = r15
    ; and [rsp+136] = user rsp.
    push qword [gs:8]               ; user rsp      -> SR_RSP
    push r11                        ; user rflags   -> SR_RFLAGS
    push rcx                        ; user rip      -> SR_RIP

    push rax                        ; SR_RAX
    push rbx                        ; SR_RBX
    push rcx                        ; SR_RCX  (user's rcx value pre-SYSCALL,
                                    ;          not the post-SYSCALL rc)
    push rdx                        ; SR_RDX
    push rsi                        ; SR_RSI
    push rdi                        ; SR_RDI
    push rbp                        ; SR_RBP
    push r8                         ; SR_R8
    push r9                         ; SR_R9
    push r10                        ; SR_R10
    push r11                        ; SR_R11  (user's r11 value pre-SYSCALL,
                                    ;          separate from rflags)
    push r12                        ; SR_R12
    push r13                        ; SR_R13
    push r14                        ; SR_R14
    push r15                        ; SR_R15  (last pushed, at [rsp+0])

    ; rsp now points at struct syscall_regs.
    mov rdi, [rsp + SR_RAX]         ; arg 1: syscall number
    mov rsi, rsp                    ; arg 2: &regs
    sti
    call syscall_handler
    cli

    ; Return path. syscall_handler updated regs->rax with the return
    ; value; regs->rip / regs->rflags / regs->rsp hold the user state.
    ; sysretq uses RCX = target rip, R11 = target rflags, RSP = target
    ; rsp (so we must pop into RSP exactly once, as the last step).
    mov rcx, [rsp + SR_RIP]         ; target rip
    mov r11, [rsp + SR_RFLAGS]      ; target rflags
    mov rax, [rsp + SR_RAX]         ; return value

    ; Validate the target rip is canonical before sysretq (sysretq
    ; #GPs on non-canonical RCX, which would leave the kernel in a
    ; broken state).
    mov rdx, rcx
    sar rdx, 47
    cmp rdx, 0
    je .canonical
    cmp rdx, -1
    je .canonical
.bad_rip:
    cli
    hlt
    jmp .bad_rip

.canonical:
    ; Pop everything except rsp. We must finish by popping user rsp
    ; last because popping it earlier would move our stack pointer
    ; to the user stack and break the subsequent pops.
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
    add rsp, 8                      ; skip SR_RAX (we loaded it above)
    add rsp, 8                      ; skip SR_RIP
    add rsp, 8                      ; skip SR_RFLAGS
    pop rsp                         ; SR_RSP -> user rsp

    swapgs
    o64 sysret