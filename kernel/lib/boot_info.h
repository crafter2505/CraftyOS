#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

#define BOOT_INFO_MAGIC 0x4352414654593031ULL
#define BOOT_INFO_MEMMAP_MAX 64

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t _pad;
} __attribute__((packed));

struct boot_info {
    uint64_t magic;
    uint32_t memmap_count;
    uint32_t _pad;
    uint64_t memmap_phys;
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint64_t kernel_phys_base;
    uint64_t kernel_phys_end;
    uint64_t pml4_phys;
} __attribute__((packed));

_Static_assert(sizeof(struct e820_entry) == 24, "E820 entry size mismatch");

#endif
