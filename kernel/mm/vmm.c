#include "vmm.h"
#include "address_space.h"
#include "pmm.h"
#include "kprintf.h"
#include "memory_map.h"

#define PML4_INDEX(v)   (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v)   (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)     (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)     (((v) >> 12) & 0x1FF)

static uint64_t current_pml4_phys = 0x10000;
static uint64_t kernel_pml4_phys = 0x10000;

static inline uint64_t *phys_to_ptr(uint64_t phys) {
    return (uint64_t *)(DIRECT_MAP_BASE + phys);
}

static uint64_t *get_current_pml4_ptr(void) {
    return (uint64_t *)(DIRECT_MAP_BASE + current_pml4_phys);
}

static uint64_t *pt_walk(uint64_t virt, int allocate, int user) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx   = PD_INDEX(virt);
    uint64_t pt_idx   = PT_INDEX(virt);

    uint64_t intermediate_flags = VMM_PRESENT | VMM_WRITABLE;
    if (user) intermediate_flags |= VMM_USER;

    uint64_t *current_pml4 = get_current_pml4_ptr();
    uint64_t pml4_entry = current_pml4[pml4_idx];
    if (!(pml4_entry & VMM_PRESENT)) {
        if (!allocate) return 0;
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) return 0;
        uint64_t *new_table = phys_to_ptr(new_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
        current_pml4[pml4_idx] = new_phys | intermediate_flags;
        pml4_entry = current_pml4[pml4_idx];
    } else if (user) {
        current_pml4[pml4_idx] = pml4_entry | VMM_USER;
        pml4_entry = current_pml4[pml4_idx];
    }
    uint64_t *pdpt = phys_to_ptr(pml4_entry & 0x000FFFFFFFFFF000ULL);

    uint64_t pdpt_entry = pdpt[pdpt_idx];
    if (!(pdpt_entry & VMM_PRESENT)) {
        if (!allocate) return 0;
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) return 0;
        uint64_t *new_table = phys_to_ptr(new_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
        pdpt[pdpt_idx] = new_phys | intermediate_flags;
        pdpt_entry = pdpt[pdpt_idx];
    } else if (user) {
        pdpt[pdpt_idx] = pdpt_entry | VMM_USER;
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
        pd[pd_idx] = new_phys | intermediate_flags;
        pd_entry = pd[pd_idx];
    } else if (user) {
        pd[pd_idx] = pd_entry | VMM_USER;
        pd_entry = pd[pd_idx];
    }

    if (pd_entry & VMM_HUGE) {
        return &pd[pd_idx];
    }
    uint64_t *pt = phys_to_ptr(pd_entry & 0x000FFFFFFFFFF000ULL);

    return &pt[pt_idx];
}

void vmm_init(void) {
    struct boot_info *boot_info = memory_map_get_boot_info();
    current_pml4_phys = boot_info ? boot_info->pml4_phys : 0x10000;
    kernel_pml4_phys = current_pml4_phys;
    kprintf("  VMM: PML4 phys 0x%x virt 0x%x\n",
            current_pml4_phys,
            (uint64_t)get_current_pml4_ptr());
}

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if ((virt & (VMM_PAGE_SIZE - 1)) != 0) return -1;
    if ((phys & (VMM_PAGE_SIZE - 1)) != 0) return -1;

    int user = (flags & VMM_USER) ? 1 : 0;
    uint64_t *entry = pt_walk(virt, 1, user);
    if (!entry) return -1;
    if (*entry & VMM_HUGE) return -1;

    *entry = (phys & 0x000FFFFFFFFFF000ULL) | flags | VMM_PRESENT;
    vmm_invlpg(virt);
    return 0;
}

void vmm_unmap(uint64_t virt) {
    if ((virt & (VMM_PAGE_SIZE - 1)) != 0) return;
    uint64_t *entry = pt_walk(virt, 0, 0);
    if (!entry) return;
    if (!(*entry & VMM_PRESENT)) return;
    *entry = 0;
    vmm_invlpg(virt);
}

uint64_t vmm_translate(uint64_t virt) {
    uint64_t pml4_entry = get_current_pml4_ptr()[PML4_INDEX(virt)];
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

int vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags) {
    return vmm_map(virt, phys, flags | VMM_USER);
}

int vmm_is_user_accessible(uint64_t virt) {
    uint64_t pml4_entry = get_current_pml4_ptr()[PML4_INDEX(virt)];
    if (!(pml4_entry & VMM_PRESENT)) return 0;
    if (!(pml4_entry & VMM_USER)) return 0;

    uint64_t *pdpt = phys_to_ptr(pml4_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pdpt_entry = pdpt[PDPT_INDEX(virt)];
    if (!(pdpt_entry & VMM_PRESENT)) return 0;
    if (!(pdpt_entry & VMM_USER)) return 0;
    if (pdpt_entry & VMM_HUGE) return 1;

    uint64_t *pd = phys_to_ptr(pdpt_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pd_entry = pd[PD_INDEX(virt)];
    if (!(pd_entry & VMM_PRESENT)) return 0;
    if (!(pd_entry & VMM_USER)) return 0;
    if (pd_entry & VMM_HUGE) return 1;

    uint64_t *pt = phys_to_ptr(pd_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pt_entry = pt[PT_INDEX(virt)];
    if (!(pt_entry & VMM_PRESENT)) return 0;
    if (!(pt_entry & VMM_USER)) return 0;

    return 1;
}

int vmm_is_user_range(uint64_t ptr, uint64_t len) {
    if (len == 0) return 1;
    if (ptr + len < ptr) return 0;
    if (ptr >= USER_SPACE_MAX) return 0;
    if (ptr + len > USER_SPACE_MAX) return 0;

    uint64_t start_page = ptr & ~0xFFFULL;
    uint64_t end_page   = (ptr + len - 1) & ~0xFFFULL;

    for (uint64_t p = start_page; p <= end_page; p += 0x1000) {
        if (!vmm_is_user_accessible(p)) {
            return 0;
        }
    }

    return 1;
}

uint64_t vmm_get_kernel_pml4(void) {
    return kernel_pml4_phys;
}

static int clone_page_table_level2(uint64_t *src_pdpt, uint64_t *dst_pdpt) {
    for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
        uint64_t src_pdpt_entry = src_pdpt[pdpt_idx];
        if (!(src_pdpt_entry & VMM_PRESENT)) continue;

        if (src_pdpt_entry & VMM_HUGE) {
            dst_pdpt[pdpt_idx] = src_pdpt_entry;
            continue;
        }

        uint64_t dst_pd_phys = (uint64_t)pmm_alloc_page();
        if (!dst_pd_phys) return -1;
        uint64_t *dst_pd = phys_to_ptr(dst_pd_phys);
        uint64_t *src_pd = phys_to_ptr(src_pdpt_entry & 0x000FFFFFFFFFF000ULL);
        for (int i = 0; i < 512; i++) dst_pd[i] = 0;

        for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
            uint64_t src_pd_entry = src_pd[pd_idx];
            if (!(src_pd_entry & VMM_PRESENT)) continue;

            if (src_pd_entry & VMM_HUGE) {
                dst_pd[pd_idx] = src_pd_entry;
                continue;
            }

            uint64_t dst_pt_phys = (uint64_t)pmm_alloc_page();
            if (!dst_pt_phys) {
                pmm_free_page((void *)dst_pd_phys);
                return -1;
            }
            uint64_t *dst_pt = phys_to_ptr(dst_pt_phys);
            uint64_t *src_pt = phys_to_ptr(src_pd_entry & 0x000FFFFFFFFFF000ULL);
            for (int i = 0; i < 512; i++) {
                dst_pt[i] = src_pt[i];
            }
            dst_pd[pd_idx] = dst_pt_phys | (src_pd_entry & 0xFFFULL);
        }

        dst_pdpt[pdpt_idx] = dst_pd_phys | (src_pdpt_entry & 0xFFFULL);
    }
    return 0;
}

uint64_t vmm_create_address_space(void) {
    uint64_t new_pml4_phys = (uint64_t)pmm_alloc_page();
    if (!new_pml4_phys) return 0;
    uint64_t *new_pml4 = (uint64_t *)(DIRECT_MAP_BASE + new_pml4_phys);
    uint64_t *kernel_pml4 = phys_to_ptr(kernel_pml4_phys);

    for (int i = 0; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }

    /* Low-half user mappings must not share page-table nodes with the kernel.
       A fork/exec operation must be able to replace or unmap user pages
       independently of every other address space. */
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        uint64_t src_pdpt_entry = kernel_pml4[pml4_idx];
        if (!(src_pdpt_entry & VMM_PRESENT)) continue;

        if (src_pdpt_entry & VMM_HUGE) {
            continue;
        }

        uint64_t dst_pdpt_phys = (uint64_t)pmm_alloc_page();
        if (!dst_pdpt_phys) {
            pmm_free_page((void *)new_pml4_phys);
            return 0;
        }
        uint64_t *dst_pdpt = phys_to_ptr(dst_pdpt_phys);
        uint64_t *src_pdpt = phys_to_ptr(src_pdpt_entry & 0x000FFFFFFFFFF000ULL);
        for (int i = 0; i < 512; i++) dst_pdpt[i] = 0;

        if (clone_page_table_level2(src_pdpt, dst_pdpt) != 0) {
            pmm_free_page((void *)dst_pdpt_phys);
            pmm_free_page((void *)new_pml4_phys);
            return 0;
        }
        new_pml4[pml4_idx] = dst_pdpt_phys | (src_pdpt_entry & 0xFFFULL);
    }

    return new_pml4_phys;
}

void vmm_destroy_address_space(uint64_t pml4_phys) {
    if (pml4_phys == 0) return;
    if (pml4_phys == kernel_pml4_phys) return;

    uint64_t *pml4_ptr = (uint64_t *)(DIRECT_MAP_BASE + pml4_phys);

    for (int i = 0; i < 256; i++) {
        uint64_t entry = pml4_ptr[i];
        if (!(entry & VMM_PRESENT)) continue;
    }

    pmm_free_page((void *)pml4_phys);
}

void vmm_switch_address_space(uint64_t pml4_phys) {
    if (pml4_phys == 0) return;
    current_pml4_phys = pml4_phys;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

uint64_t vmm_get_current_pml4(void) {
    return current_pml4_phys;
}