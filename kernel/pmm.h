#ifndef PMM_H
#define PMM_H

#include "types.h"

#define PAGE_SIZE       4096

#define PMM_MAX_PAGES   (4 * 1024 * 1024)
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)

#define PMM_ALLOC_LIMIT (1024 * 1024 / 4)

void pmm_init(void);

void *pmm_alloc_page(void);
void pmm_free_page(void *addr);

uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_free_pages(void);

#endif