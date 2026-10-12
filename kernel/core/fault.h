#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>
#include "isr.h"

void fault_report(uint64_t int_no, uint64_t err_code, struct registers *regs)
    __attribute__((noreturn));

void fault_print_page_error(uint64_t err);

uint64_t fault_read_cr2(void);

uint64_t fault_read_cr3(void);

#endif