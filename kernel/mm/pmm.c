#include "pmm.h"
#include "memory_map.h"
#include "kprintf.h"
#include "address_space.h"
#include "buddy.h"

static uint8_t bitmap[PMM_BITMAP_SIZE];

static uint64_t total_pages = 0;
static uint64_t free_page_count = 0;

extern char kernel_end;


static inline void bitmap_set(uint64_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bitmap_test(uint64_t page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

static void pmm_mark_used(uint64_t base, uint64_t size) {
    uint64_t start = base / PAGE_SIZE;
    uint64_t end = (base + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t p = start; p < end && p < PMM_MAX_PAGES; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            if (free_page_count > 0) free_page_count--;
        }
    }
}

static void pmm_mark_free(uint64_t base, uint64_t size) {
    uint64_t start = (base + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t end = (base + size) / PAGE_SIZE;

    for (uint64_t p = start; p < end && p < PMM_MAX_PAGES; p++) {
        if (bitmap_test(p)) {
            bitmap_clear(p);
            free_page_count++;
        }
    }
}

void *pmm_alloc_page(void) {
    return alloc_pages(0);
}

void pmm_free_page(void *addr) {
    free_pages(addr, 0);
}

void pmm_init(void) {
    for (uint64_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        bitmap[i] = 0xFF;
    }
    free_page_count = 0;
    total_pages = 0;

    uint32_t count = MEMORY_MAP_COUNT;
    for (uint32_t i = 0; i < count; i++) {
        struct e820_entry *e = &MEMORY_MAP_ENTRIES[i];


        if (e->type == E820_TYPE_USABLE && e->length > 0) {
            pmm_mark_free(e->base, e->length);
        }
    }

    total_pages = free_page_count;
    kprintf("  PMM: found %d usable pages (%d MB)\n",
            (long)total_pages,
            (long)(total_pages * PAGE_SIZE / (1024 * 1024)));

    pmm_mark_used(0, 0x100000);

    uint64_t kernel_end_phys = kvirt_to_phys(&kernel_end);
    pmm_mark_used(0x100000, kernel_end_phys - 0x100000);

    kprintf("  PMM: %d pages free after reserving kernel\n",
            (long)free_page_count);
}

uint64_t pmm_get_total_pages(void) {
    return total_pages;
}

uint64_t pmm_get_free_pages(void) {
    return free_page_count;
}