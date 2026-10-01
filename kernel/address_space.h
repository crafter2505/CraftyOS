#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include "types.h"

#define KERNEL_VIRT_BASE    0xFFFFFFFF80000000ULL
#define KERNEL_PHYS_BASE    0x0000000000200000ULL

#define DIRECT_MAP_BASE     0xFFFF800000000000ULL

#define USER_SPACE_LIMIT    0x0000800000000000ULL

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(DIRECT_MAP_BASE + phys);
}

static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - DIRECT_MAP_BASE;
}

static inline uint64_t kvirt_to_phys(void *virt) {
    return (uint64_t)virt - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
}

static inline void *kphys_to_virt(uint64_t phys) {
    return (void *)(phys - KERNEL_PHYS_BASE + KERNEL_VIRT_BASE);
}

#endif