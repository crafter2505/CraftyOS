BITS 64
global syscall_entry

section .text
syscall_entry:
    sysret
