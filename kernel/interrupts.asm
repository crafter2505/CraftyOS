BITS 64

extern isr_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0            ; dummy error code
    push qword %1           ; interrupt number
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NOERR 0     ; Divide Error
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound Range
ISR_NOERR 6     ; Invalid Opcode
ISR_NOERR 7     ; Device Not Available
ISR_ERR   8     ; Double Fault
ISR_NOERR 9     ; Coprocessor Segment Overrun
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment Not Present
ISR_ERR   12    ; Stack Fault
ISR_ERR   13    ; General Protection Fault
ISR_ERR   14    ; Page Fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 FPU Error
ISR_ERR   17    ; Alignment Check
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD FPU Error
ISR_NOERR 20    ; Virtualization
ISR_ERR   21    ; Control Protection
ISR_NOERR 22    ; Reserved
ISR_NOERR 23    ; Reserved
ISR_NOERR 24    ; Reserved
ISR_NOERR 25    ; Reserved
ISR_NOERR 26    ; Reserved
ISR_NOERR 27    ; Reserved
ISR_NOERR 28    ; Hypervisor Injection
ISR_ERR   29    ; VMM Communication
ISR_ERR   30    ; Security Exception
ISR_NOERR 31    ; Reserved


%assign i 32
%rep 224
    ISR_NOERR i
%assign i i+1
%endrep

isr_common:
    ; Push all general-purpose registers
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

    mov rdi, [rsp + 120]    ; interrupt number (at the end of the pushes)
    mov rsi, [rsp + 128]    ; error code
    mov rdx, rsp            ; pointer to register block

    call isr_handler

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
    pop rax

    add rsp, 16
    iretq