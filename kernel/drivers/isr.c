#include "isr.h"
#include "kprintf.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "fault.h"
#include "sched.h"

void isr_handler(uint64_t int_no, uint64_t err_code, struct registers *regs)
{
    if (int_no < 32) {
        fault_report(int_no, err_code, regs);
    } else if (int_no < 48) {
        uint64_t irq = int_no - 32;
        switch (irq) {
        case 0: timer_tick(); break;
        case 1: keyboard_handler(); break;
        default: break;
        }
        pic_send_eoi((unsigned char)irq);
        if (irq == 0) sched_tick();
    } else {
        kprintf("[INT %d]\n", (long)int_no);
    }
}