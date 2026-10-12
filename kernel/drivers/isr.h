#ifndef ISR_H
#define ISR_H

#include "types.h"

struct registers
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

void isr_handler(uint64_t int_no, uint64_t err_code,struct registers *regs);

#endif