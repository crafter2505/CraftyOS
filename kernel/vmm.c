#include "vmm.h"
#include "address_space.h"
#include "pmm.h"
#include "kprintf.h"

#define PML4_INDEX(v)   (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v)   (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)     (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)     (((v) >> 12) & 0x1FF)

static uint64_t *pml4 = 0;

static inline uint64_t *phys_to_ptr(uint64_t phys) {
    return (uint64_t *)(DIRECT_MAP_BASE + phys);
}

static uint64_t *pt_walk(uint64_t virt, int allocate) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    uint64_t pml4_entry = pml4[pml4_idx];
    if (!(pml4_entry & VMM_PRESENT)) {
        if (!allocate) return 0;
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) return 0;
        uint64_t *new_table = phys_to_ptr(new_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
        pml4[pml4_idx] = new_phys | VMM_PRESENT | VMM_WRITABLE;
        pml4_entry = pml4[pml4_idx];
    }
    uint64_t *pdpt = phys_to_ptr(pml4_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pdpt_entry = pdpt[pdpt_idx];
    if (!(pdpt_entry & VMM_PRESENT)) {
        if (!allocate) return 0;
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) return 0;
        uint64_t *new_table = phys_to_ptr(new_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
        pdpt[pdpt_idx] = new_phys | VMM_PRESENT | VMM_WRITABLE;
        pdpt_entry = pdpt[pdpt_idx];
    }
    uint64_t *pd = phys_to_ptr(pdpt_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pd_entry = pd[pd_idx];
    if (!(pd_entry & VMM_PRESENT)) {
        if (!allocate) return 0;
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) return 0;
        uint64_t *new_table = phys_to_ptr(new_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
        pd[pd_idx] = new_phys | VMM_PRESENT | VMM_WRITABLE;
        pd_entry = pd[pd_idx];
    }

    if (pd_entry & VMM_HUGE) {
        return &pd[pd_idx];
    }
    uint64_t *pt = phys_to_ptr(pd_entry & 0x000FFFFFFFFFF000ULL);

    return &pt[pt_idx];
}

void vmm_init(void) {
    pml4 = phys_to_ptr(0x10000);
    kprintf("  VMM: PML4 at virt 0x%x (phys 0x10000)\n", (uint64_t)pml4);
}

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if ((virt & (VMM_PAGE_SIZE - 1)) != 0) return -1;
    if ((phys & (VMM_PAGE_SIZE - 1)) != 0) return -1;

    uint64_t *entry = pt_walk(virt, 1);
    if (!entry) return -1;

    if (*entry & VMM_HUGE) return -1;

    *entry = (phys & 0x000FFFFFFFFFF000ULL) | flags | VMM_PRESENT;
    vmm_invlpg(virt);
    return 0;
}


void vmm_unmap(uint64_t virt) {
    if ((virt & (VMM_PAGE_SIZE - 1)) != 0) return;
    uint64_t *entry = pt_walk(virt, 0);
    if (!entry) return;
    if (!(*entry & VMM_PRESENT)) return;
    *entry = 0;
    vmm_invlpg(virt);
}

uint64_t vmm_translate(uint64_t virt) {
    uint64_t pml4_entry = pml4[PML4_INDEX(virt)];
    if (!(pml4_entry & VMM_PRESENT)) return 0;
    uint64_t *pdpt = phys_to_ptr(pml4_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pdpt_entry = pdpt[PDPT_INDEX(virt)];
    if (!(pdpt_entry & VMM_PRESENT)) return 0;
    if (pdpt_entry & VMM_HUGE) {
        uint64_t base = pdpt_entry & 0x000FFFFFC0000000ULL;
        return base + (virt & 0x3FFFFFFF);
    }
    uint64_t *pd = phys_to_ptr(pdpt_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pd_entry = pd[PD_INDEX(virt)];
    if (!(pd_entry & VMM_PRESENT)) return 0;
    if (pd_entry & VMM_HUGE) {
        uint64_t base = pd_entry & 0x000FFFFFFFE00000ULL;
        return base + (virt & 0x1FFFFF);
    }
    uint64_t *pt = phys_to_ptr(pd_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pt_entry = pt[PT_INDEX(virt)];
    if (!(pt_entry & VMM_PRESENT)) return 0;
    uint64_t base = pt_entry & 0x000FFFFFFFFFF000ULL;
    return base + (virt & 0xFFF);
}

uint64_t vmm_alloc_page(uint64_t virt, uint64_t flags) {
    uint64_t phys = (uint64_t)pmm_alloc_page();
    if (!phys) return 0;

    if (vmm_map(virt, phys, flags) != 0) {
        pmm_free_page((void *)phys);
        return 0;
    }
    return virt;
}

void vmm_invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}