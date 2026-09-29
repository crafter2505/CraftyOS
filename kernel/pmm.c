#include "pmm.h"
#include "memory_map.h"
#include "kprintf.h"
#include "address_space.h"

static uint8_t bitmap[PMM_BITMAP_SIZE];

static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

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

static uint64_t last_alloc_hint = 0;

static void pmm_mark_used(uint64_t base, uint64_t size) {
    uint64_t start = base / PAGE_SIZE;
    uint64_t end = (base + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t p = start; p < end && p < PMM_MAX_PAGES; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            if (free_pages > 0) free_pages--;
        }
    }
}

static void pmm_mark_free(uint64_t base, uint64_t size) {
    uint64_t start = (base / PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t end = (base + size) / PAGE_SIZE;

    for (uint64_t p = start; p < end && p < PMM_MAX_PAGES; p++) {
        if (bitmap_test(p)) {
            bitmap_clear(p);
            free_pages++;
        }
    }
}

void *pmm_alloc_page(void) {
    if (free_pages == 0) {
        return 0;
    }

    for (int pass = 0; pass < 2; pass++) {
        uint64_t start = (pass == 0) ? last_alloc_hint : 0;
        uint64_t end = (pass == 0) ? PMM_ALLOC_LIMIT : last_alloc_hint;

        if (start >= PMM_ALLOC_LIMIT) {
            continue;
        }

        for (uint64_t p = start; p < end; p++) {
            if (!bitmap_test(p)) {
                bitmap_set(p);
                free_pages--;
                last_alloc_hint = p + 1;
                return (void *)(p * PAGE_SIZE);
            }
        }
    }
    return 0;
}

void pmm_free_page(void *addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;

    if (page >= PMM_MAX_PAGES) {
        return;
    }

    if (!bitmap_test(page)) {
        return;
    }

    bitmap_clear(page);
    free_pages++;

    if (page < last_alloc_hint) {
        last_alloc_hint = page;
    }
}

void pmm_init(void) {
    for (uint64_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        bitmap[i] = 0xFF;
    }
    free_pages = 0;
    total_pages = 0;

    uint32_t count = MEMORY_MAP_COUNT;
    for (uint32_t i = 0; i < count; i++) {
        struct e820_entry *e = &MEMORY_MAP_ENTRIES[i];


        if (e->type == E820_TYPE_USABLE && e->length > 0) {
            pmm_mark_free(e->base, e->length);
        }
    }

    total_pages = free_pages;
    kprintf("  PMM: found %d usable pages (%d MB)\n",
            (long)total_pages,
            (long)(total_pages * PAGE_SIZE / (1024 * 1024)));

    pmm_mark_used(0, 0x100000);

    uint64_t kernel_end_phys = kvirt_to_phys(&kernel_end);
    pmm_mark_used(0x100000, kernel_end_phys - 0x100000);

    kprintf("  PMM: %d pages free after reserving kernel\n",
            (long)free_pages);
}

uint64_t pmm_get_total_pages(void) {
    return total_pages;
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}