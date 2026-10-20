#include "memory_map.h"
#include "kprintf.h"

static struct boot_info *active_boot_info;

uint32_t memory_map_count(void) {
    if (active_boot_info) return active_boot_info->memmap_count;
    return *(volatile uint32_t *)0x500;
}

struct e820_entry *memory_map_entries(void) {
    if (active_boot_info) {
        return (struct e820_entry *)(uintptr_t)active_boot_info->memmap_phys;
    }
    return (struct e820_entry *)0x508;
}

int memory_map_set_boot_info(struct boot_info *boot_info) {
    if (!boot_info || boot_info->magic != BOOT_INFO_MAGIC ||
        boot_info->memmap_count == 0 ||
        boot_info->memmap_count > BOOT_INFO_MEMMAP_MAX ||
        boot_info->memmap_phys == 0 || boot_info->pml4_phys == 0) {
        return -1;
    }
    active_boot_info = boot_info;
    return 0;
}

struct boot_info *memory_map_get_boot_info(void) {
    return active_boot_info;
}

void memory_map_dump(void) {
    uint32_t count = MEMORY_MAP_COUNT;
    uint64_t total_usable = 0;

    kprintf("Memory map: %d entries\n", (long)count);

    for (uint32_t i = 0; i < count; i++) {
        struct e820_entry *e = &MEMORY_MAP_ENTRIES[i];

        kprintf("  [%d] base=0x%x len=0x%x type=0x%d\n",
                (long)i, e->base, e->length, (long)e->type);
        if (e->type == E820_TYPE_USABLE) {
            total_usable += e->length;
        }
    }

    kprintf(" Total usable: %d MB\n",
    (long)(total_usable / (1024 * 1024)));
}