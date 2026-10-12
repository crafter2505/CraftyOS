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

void vmm_test(void) {
    kprintf("\nVMM Test:\n");

    uint64_t test_virt = 0xDEADB000ULL;
    uint64_t phys1 = (uint64_t)pmm_alloc_page();
    kprintf(" [1] Allocated phys page: 0x%x\n", phys1);

    if (vmm_map(test_virt, phys1, VMM_KERNEL_RW) != 0) {
        kprintf("  [1] FAIL: vmm_map returned error\n");
        pmm_free_page((void *)phys1);
        return;
    }

    volatile uint64_t *test = (volatile uint64_t *)test_virt;
    *test = 0xCAFEBABE12345678ULL;
    uint64_t read_back = *test;
    uint64_t translated = vmm_translate(test_virt);

    if (read_back == 0xCAFEBABE12345678ULL && translated == phys1) {
        kprintf("  [1] PASS: map/read/translate roundtrip\n");
    } else {
        kprintf("  [1] FAIL: read=0x%x translated=0x%x\n", read_back, translated);
    }

    vmm_unmap(test_virt);
    pmm_free_page((void *)phys1);

     uint64_t base = 0xC0DE0000ULL;
    uint64_t p1 = (uint64_t)pmm_alloc_page();
    uint64_t p2 = (uint64_t)pmm_alloc_page();
    uint64_t p3 = (uint64_t)pmm_alloc_page();

    vmm_map(base,           p1, VMM_KERNEL_RW);
    vmm_map(base + 0x1000,  p2, VMM_KERNEL_RW);
    vmm_map(base + 0x2000,  p3, VMM_KERNEL_RW);

       int ok = 1;
    if (vmm_translate(base)          != p1) ok = 0;
    if (vmm_translate(base + 0x1000) != p2) ok = 0;
    if (vmm_translate(base + 0x2000) != p3) ok = 0;

    vmm_unmap(base + 0x1000);

    if (vmm_translate(base + 0x1000) != 0) ok = 0;
    if (vmm_translate(base)          != p1) ok = 0;
    if (vmm_translate(base + 0x2000) != p3) ok = 0;

    if (ok) {
        kprintf("  [2] PASS: three pages, middle unmapped\n");
    } else {
        kprintf("  [2] FAIL\n");
    }

    vmm_unmap(base);
    vmm_unmap(base + 0x2000);
    pmm_free_page((void *)p1);
    pmm_free_page((void *)p2);
    pmm_free_page((void *)p3);

    uint64_t huge = vmm_translate(0x100000);
    if (huge == 0x100000) {
        kprintf("  [3] PASS: 2 MB huge page translation\n");
    } else {
        kprintf("  [3] FAIL: 0x100000 -> 0x%x\n", huge);
    }

    uint64_t unmapped = vmm_translate(0xBAD000000ULL);
    if (unmapped == 0) {
        kprintf("  [4] PASS: unmapped address returns 0\n");
    } else {
        kprintf("  [4] FAIL: 0xBAD000000 -> 0x%x\n", unmapped);
    }

    kprintf("VMM test complete.\n");
}

void vmm_invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}