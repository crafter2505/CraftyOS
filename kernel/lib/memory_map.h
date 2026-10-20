#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "types.h"
#include "boot_info.h"

#define MEMORY_MAP_ADDR     0x500
#define MEMORY_MAP_COUNT    (memory_map_count())
#define MEMORY_MAP_ENTRIES  (memory_map_entries())

#define E820_TYPE_USABLE    1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI      3
#define E820_TYPE_NVS       4
#define E820_TYPE_UNUSABLE  5

void memory_map_dump(void);
uint32_t memory_map_count(void);
struct e820_entry *memory_map_entries(void);
int memory_map_set_boot_info(struct boot_info *boot_info);
struct boot_info *memory_map_get_boot_info(void);

#endif