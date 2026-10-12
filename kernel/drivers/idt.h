#ifndef IDT_H
#define IDT_H

#include "types.h"

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_descriptor {
    uint16_t size;
    uint64_t address;
} __attribute__((packed));

void idt_init(void);

void idt_set_entry(int vector, void (*handler)(void), uint8_t type_attr);

#endif