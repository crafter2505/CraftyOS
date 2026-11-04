#ifndef SYSCALL_REGS_H
#define SYSCALL_REGS_H

#include "types.h"

/*
 * Layout of the register frame that syscall_entry pushes before calling
 * syscall_handler. MUST match the push order in kernel/arch/syscall.asm:
 *
 *   push [gs:8]     ; user rsp
 *   push r11        ; user rflags
 *   push rcx        ; user rip
 *   push rax
 *   push rbx
 *   push rcx        ; (again, rcx saved twice; see below)
 *   push rdx
 *   push rsi
 *   push rdi
 *   push rbp
 *   push r8
 *   push r9
 *   push r10
 *   push r11
 *   push r12
 *   push r13
 *   push r14
 *   push r15
 *
 * After syscall_entry pushes all of the above, rsp points at the saved
 * r15. Because `push` decrements rsp, the *first* pushed value is at
 * the *highest* address. In the struct, that means fields are laid out
 * from lowest address (= last push = r15) to highest (= first push =
 * user rsp):
 *
 *   offset   0 : r15     (last pushed, at [rsp+0])
 *   offset   8 : r14
 *   offset  16 : r13
 *   offset  24 : r12
 *   offset  32 : r11
 *   offset  40 : r10
 *   offset  48 : r9
 *   offset  56 : r8
 *   offset  64 : rbp
 *   offset  72 : rdi
 *   offset  80 : rsi
 *   offset  88 : rdx
 *   offset  96 : rcx
 *   offset 104 : rbx
 *   offset 112 : rax     (syscall number read here by entry stub)
 *   offset 120 : rip     (user, after SYSCALL)
 *   offset 128 : rflags  (user RFLAGS at SYSCALL)
 *   offset 136 : rsp     (user RSP before SYSCALL)
 *
 * NOTE: SYSCALL does NOT push int_no/err_code (unlike IRQ/exception
 * frames). Do NOT add them here or in syscall.asm without updating
 * the handler's expectations and the pop sequence in syscall.asm.
 *
 * SYSCALL also does not restore rcx/r11 automatically; rcx holds the
 * user rip and r11 holds the user rflags at entry. They're both saved
 * twice (once as user context, once as GPRs) because syscall_entry
 * pushes the special registers *and* the full GPR set. This is
 * intentional — the "user rip" slot must survive register mutation
 * during the handler (e.g. rcx is caller-saved and C code will clobber
 * it), and the "GPR rcx" slot must hold the value the user program had
 * in rcx before the syscall.
 */
struct syscall_regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
};

#endif