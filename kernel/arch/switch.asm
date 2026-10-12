BITS 64
global switch_to
section .text

; void switch_to(struct thread_context *old_ctx, struct thread_context *new_ctx)
;
; Struct offsets (must match kernel/thread.h):
;   r15   0    r14   8    r13  16    r12  24
;   r11  32    r10  40    r9   48    r8   56
;   rbp  64    rdi  72    rsi  80    rdx  88
;   rcx  96    rbx 104    rax 112
;   rip 120    rflags 128 rsp 136
switch_to:
    ; -------- Save current context into *rdi --------
    mov [rdi +   0], r15
    mov [rdi +   8], r14
    mov [rdi +  16], r13
    mov [rdi +  24], r12
    mov [rdi +  32], r11
    mov [rdi +  40], r10
    mov [rdi +  48], r9
    mov [rdi +  56], r8
    mov [rdi +  64], rbp
    mov [rdi +  72], rdi
    mov [rdi +  80], rsi
    mov [rdi +  88], rdx
    mov [rdi +  96], rcx
    mov [rdi + 104], rbx
    mov [rdi + 112], rax

    ; Saved rip = return address
    mov rax, [rsp]
    mov [rdi + 120], rax

    ; Saved rflags
    pushfq
    pop rax
    mov [rdi + 128], rax

    ; Saved rsp = rsp as if switch_to had returned
    lea rax, [rsp + 8]
    mov [rdi + 136], rax

    ; -------- Load new context from *rsi --------
    ; Switch stacks first (this also preserves rsi: it's a register,
    ; not affected by rsp change)
    mov rsp, [rsi + 136]

    ; Push the entry frame: rflags on top, rip below
    push qword [rsi + 120]      ; RIP (pushed first → deeper)
    push qword [rsi + 128]      ; RFLAGS (pushed last → on top)

    ; Load GPRs. Order matters: rsi is loaded LAST since it's the
    ; base pointer for all the loads.
    mov r15, [rsi +   0]
    mov r14, [rsi +   8]
    mov r13, [rsi +  16]
    mov r12, [rsi +  24]
    mov r11, [rsi +  32]
    mov r10, [rsi +  40]
    mov r9,  [rsi +  48]
    mov r8,  [rsi +  56]
    mov rbp, [rsi +  64]
    mov rdi, [rsi +  72]
    ; skip rsi here — load it at the end
    mov rdx, [rsi +  88]
    mov rcx, [rsi +  96]
    mov rbx, [rsi + 104]
    mov rax, [rsi + 112]

    ; Load rsi last
    mov rsi, [rsi +  80]

    ; Restore flags and jump
    popfq
    ret