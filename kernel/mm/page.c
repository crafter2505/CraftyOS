#include "page.h"
#include "address_space.h"

extern struct page page_array[];
extern uint64_t page_array_size;

struct page *page_from_phys(uint64_t phys) {
    uint64_t idx = phys / 4096;
    if (idx >= page_array_size) return 0;
    return &page_array[idx];
}

struct page *page_from_virt(uint64_t virt) {
    if (virt < DIRECT_MAP_BASE) return 0;
    uint64_t phys = virt - DIRECT_MAP_BASE;
    return page_from_phys(phys);
}

uint64_t page_to_phys(struct page *p) {
    uint64_t idx = p - page_array;
    return idx * 4096;
}

uint64_t page_to_virt(struct page *p) {
    return DIRECT_MAP_BASE + page_to_phys(p);
}
