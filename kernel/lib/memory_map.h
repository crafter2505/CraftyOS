#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "types.h"

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
}__attribute__((packed));

#define MEMORY_MAP_ADDR     0x500
#define MEMORY_MAP_COUNT    (*(volatile uint32_t *)0x500)
#define MEMORY_MAP_ENTRIES  ((struct e820_entry *)0x508)

#define E820_TYPE_USABLE    1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI      3
#define E820_TYPE_NVS       4
#define E820_TYPE_UNUSABLE  5

void memory_map_dump(void);

#endif