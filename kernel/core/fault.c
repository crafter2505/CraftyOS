#include "fault.h"
#include "kprintf.h"
#include "terminal.h"

uint64_t fault_read_cr2(void)
{
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

uint64_t fault_read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void fault_print_page_error(uint64_t err)
{
    kprintf("    Present:  %s\n",
            (err & 0x1) ? "protection violation" : "not-present page");
    kprintf("    Access:   %s\n",
            (err & 0x2) ? "write" : "read");
    kprintf("    Mode:     %s\n",
            (err & 0x4) ? "user" : "supervisor");
    kprintf("    Reserved: %s\n",
            (err & 0x8) ? "reserved bit set" : "no");
    kprintf("    I-fetch:  %s\n",
            (err & 0x10) ? "instruction fetch" : "data access");
    if (err & 0x20) {
        kprintf("    PK:       protection key violation\n");
    }
    if (err & 0x40) {
        kprintf("    SS:       shadow stack access\n");
    }
}

void fault_report(uint64_t int_no, uint64_t err_code, struct registers *regs)
{
    terminal_set_color(VGA_LIGHT_RED, VGA_BLACK);
    kprintf("\n[FAULT] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    const char *name = "Unknown";
    switch (int_no) {
    case 0:  name = "Divide Error"; break;
    case 1:  name = "Debug"; break;
    case 2:  name = "NMI"; break;
    case 3:  name = "Breakpoint"; break;
    case 4:  name = "Overflow"; break;
    case 5:  name = "Bound Range Exceeded"; break;
    case 6:  name = "Invalid Opcode"; break;
    case 7:  name = "Device Not Available"; break;
    case 8:  name = "Double Fault"; break;
    case 9:  name = "Coprocessor Segment Overrun"; break;
    case 10: name = "Invalid TSS"; break;
    case 11: name = "Segment Not Present"; break;
    case 12: name = "Stack-Segment Fault"; break;
    case 13: name = "General Protection Fault"; break;
    case 14: name = "Page Fault"; break;
    case 16: name = "x87 Floating-Point"; break;
    case 17: name = "Alignment Check"; break;
    case 18: name = "Machine Check"; break;
    case 19: name = "SIMD Floating-Point"; break;
    case 20: name = "Virtualization"; break;
    case 21: name = "Control Protection"; break;
    case 30: name = "Security Exception"; break;
    }
    kprintf("%s (%d)\n", name, (long)int_no);
    kprintf("----------------------------------------\n");

    kprintf("  RIP:    0x%x\n", regs->rip);
    kprintf("  CS:     0x%x\n", regs->cs);
    kprintf("  RFLAGS: 0x%x\n", regs->rflags);
    kprintf("  RSP:    0x%x\n", regs->rsp);
    kprintf("  SS:     0x%x\n", regs->ss);
    kprintf("  RAX:    0x%x\n", regs->rax);
    kprintf("  RBX:    0x%x\n", regs->rbx);
    kprintf("  RCX:    0x%x\n", regs->rcx);
    kprintf("  RDX:    0x%x\n", regs->rdx);
    kprintf("  RSI:    0x%x\n", regs->rsi);
    kprintf("  RDI:    0x%x\n", regs->rdi);
    kprintf("  RBP:    0x%x\n", regs->rbp);

    if (int_no == 14) {
        uint64_t cr2 = fault_read_cr2();
        uint64_t cr3 = fault_read_cr3();
        kprintf("----------------------------------------\n");
        kprintf("  CR2 (fault addr): 0x%x\n", cr2);
        kprintf("  CR3 (pml4 base):  0x%x\n", cr3);
        kprintf("----------------------------------------\n");
        kprintf("  Error code: 0x%x\n", err_code);
        fault_print_page_error(err_code);
    } else if (int_no == 8 || int_no == 10 || int_no == 11 ||
               int_no == 12 || int_no == 13 || int_no == 17) {
        kprintf("----------------------------------------\n");
        kprintf("  Error code: 0x%x\n", err_code);
    }

    kprintf("----------------------------------------\n");
    terminal_set_color(VGA_LIGHT_RED, VGA_BLACK);
    kprintf("System halted.\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}