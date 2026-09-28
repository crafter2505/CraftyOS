#ifndef PMM_H
#define PMM_H

#include "types.h"

#define PAGE_SIZE   4096

#define PMM_MAX_PAGES (1024 * 1024)
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)

void pmm_init(void);

uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_free_pages(void);

#endif