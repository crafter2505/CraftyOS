#ifndef VMM_H
#define VMM_H

#include "types.h"

#define VMM_PAGE_SIZE   4096

#define VMM_PRESENT     (1ULL << 0)
#define VMM_WRITABLE    (1ULL << 1)
#define VMM_USER        (1ULL << 2)
#define VMM_HUGE        (1ULL << 7)

#define VMM_KERNEL_RW   (VMM_PRESENT | VMM_WRITABLE)
#define VMM_KERNEL_RO   (VMM_PRESENT)
#define VMM_USER_RW     (VMM_PRESENT | VMM_WRITABLE | VMM_USER)

void vmm_init(void);

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_is_user_accessible(uint64_t virt);

void vmm_unmap(uint64_t virt);

void vmm_invlpg(uint64_t virt);

uint64_t vmm_translate(uint64_t virt);

uint64_t vmm_alloc_page(uint64_t virt, uint64_t flags);

void vmm_test(void);

#endif