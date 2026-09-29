#include "vmm.h"
#include "address_space.h"
#include "kprintf.h"

#define PML4_INDEX(v)   (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v)   (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)     (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)     (((v) >> 12) & 0x1FF)


static uint64_t *pml4 = 0;

static inline uint64_t *phys_to_ptr(uint64_t phys) {
    return (uint64_t *)(DIRECT_MAP_BASE + phys);
}

static inline uint64_t ptr_to_phys(uint64_t *ptr) {
    return (uint64_t)ptr - DIRECT_MAP_BASE;
}

void vmm_init(void) {
    pml4 = phys_to_ptr(0x10000);
    kprintf("  VMM: PML4 at virt 0x%x (phys 0x10000)\n", (uint64_t)pml4);
}

static uint64_t pt_lookup(uint64_t *table, int index) {
    return table[index];
}

static int pt_present(uint64_t entry) {
    return (entry & VMM_PRESENT) != 0;
}

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)virt; (void)phys; (void)flags;
    return -1;  /* not implemented */
}

void vmm_unmap(uint64_t virt) {
    (void)virt;
}

uint64_t vmm_translate(uint64_t virt) {
    (void)virt;
    return 0;
}

uint64_t vmm_alloc_page(uint64_t virt, uint64_t flags) {
    (void)virt; (void)flags;
    return 0;
}