#include "memory_map.h"
#include "kprintf.h"

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