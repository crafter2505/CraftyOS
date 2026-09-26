#include "isr.h"
#include "kprintf.h"
#include "terminal.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"


static const char *exception_names[] = {
    "Divide Error",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD FPU Error",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Hypervisor Injection",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

void isr_handler(uint64_t int_no, uint64_t err_code, struct registers *regs) {
    if (int_no < 32) {
        terminal_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("\n[EXCEPTION] ");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
        kprintf("%s (%d)\n", exception_names[int_no], (int)int_no);
        kprintf("  Error code: 0x%x\n", err_code);
        kprintf("  RIP:        0x%x\n", regs->rip);
        kprintf("  CS:         0x%x\n", regs->cs);
        kprintf("  RFLAGS:     0x%x\n", regs->rflags);
        kprintf("  RSP:        0x%x\n", regs->rsp);
        kprintf("  SS:         0x%x\n", regs->ss);
        kprintf("\nSystem halted.\n");

        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    } else if (int_no < 48) {
        uint64_t irq = int_no - 32;

        switch (irq) {
        case 0:
            timer_tick();
            break;
        case 1:  
            keyboard_handler();
            break;
        default:
            break;
        }

        pic_send_eoi((unsigned char)irq);
    } else {
        kprintf("[INT %d]\n", (int)int_no);
    }
}