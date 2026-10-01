#include "buddy.h"
#include "kprintf.h"
#include "memory_map.h"
#include "address_space.h"

static struct page page_array[BUDDY_MAX_PAGES];
static struct page *free_lists[BUDDY_MAX_ORDER + 1];

static uint64_t total_managed_pages = 0;
static uint64_t free_page_count = 0;
static uint64_t free_block_counts[BUDDY_MAX_ORDER + 1] = {0};

static uint64_t highest_phys_addr = 0;

static struct page *phys_to_page(uint64_t phys) {
    uint64_t page_idx = phys / BUDDY_PAGE_SIZE;
    if (page_idx >= BUDDY_MAX_PAGES) return 0;
    return &page_array[page_idx];
}

static uint64_t page_to_phys(struct page *p) {
    return (uint64_t)(p - page_array) * BUDDY_PAGE_SIZE;
}
static void free_list_push(struct page *p) {
    uint8_t order = p->order;
    p->flags = PAGE_FREE;
    p->next = free_lists[order];
    p->prev = 0;
    if (free_lists[order]) {
        free_lists[order]->prev = p;
    }
    free_lists[order] = p;
    free_block_counts[order]++;
    free_page_count += (1ULL << order);
}

static void free_list_remove(struct page *p) {
    uint8_t order = p->order;
    if (p->prev) {
        p->prev->next = p->next;
    } else {
        free_lists[order] = p->next;
    }
    if (p->next) {
        p->next->prev = p->prev;
    }
    p->next = 0;
    p->prev = 0;
    free_block_counts[order]--;
    free_page_count -= (1ULL << order);
}

static void mark_reserved(uint64_t base, uint64_t size) {
    uint64_t start_page = base / BUDDY_PAGE_SIZE;
    uint64_t end_page = (base + size + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;

    if (end_page > BUDDY_MAX_PAGES) end_page = BUDDY_MAX_PAGES;

    for (uint64_t i = start_page; i < end_page; i++) {
        page_array[i].flags = PAGE_UNAVAILABLE;
        page_array[i].order = 0;
        page_array[i].next = 0;
        page_array[i].prev = 0;
    }
}

static void mark_free_range(uint64_t base, uint64_t size) {
    uint64_t start_page = (base + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
    uint64_t end_page = (base + size) / BUDDY_PAGE_SIZE;

    if (end_page > BUDDY_MAX_PAGES) end_page = BUDDY_MAX_PAGES;
    if (start_page >= end_page) return;

    for (uint64_t i = start_page; i < end_page; i++) {
        page_array[i].flags = PAGE_ALLOCATED;
        page_array[i].order = 0;
        page_array[i].next = 0;
        page_array[i].prev = 0;
    }
    total_managed_pages += (end_page - start_page);
}


static int max_order_for(uint64_t start_page, uint64_t count) {
    int order = 0;
    uint64_t size = 1;
    while (order < BUDDY_MAX_ORDER && size * 2 <= count) {
        if (start_page % (size * 2) != 0) break;
        size *= 2;
        order++;
    }
    return order;
}

static void build_buddy_from_range(uint64_t start_page, uint64_t end_page) {
    uint64_t cur = start_page;
    while (cur < end_page) {
        uint64_t remaining = end_page - cur;
        int order = max_order_for(cur, remaining);
        struct page *p = &page_array[cur];
        p->order = (uint8_t)order;
        free_list_push(p);
        cur += (1ULL << order);
    }
}

void buddy_init(void) {
    for (uint64_t i = 0; i < BUDDY_MAX_PAGES; i++) {
        page_array[i].flags = PAGE_UNAVAILABLE;
        page_array[i].order = 0;
        page_array[i].next = 0;
        page_array[i].prev = 0;
    }
    for (int o = 0; o <= BUDDY_MAX_ORDER; o++) {
        free_lists[o] = 0;
        free_block_counts[o] = 0;
    }
    total_managed_pages = 0;
    free_page_count = 0;
    highest_phys_addr = 0;

    uint32_t count = MEMORY_MAP_COUNT;
    for (uint32_t i = 0; i < count; i++) {
        struct e820_entry *e = &MEMORY_MAP_ENTRIES[i];
        if (e->type != E820_TYPE_USABLE) continue;
        if (e->length == 0) continue;

        uint64_t end = e->base + e->length;
        if (end > highest_phys_addr) highest_phys_addr = end;

        mark_free_range(e->base, e->length);
    }

    mark_reserved(0, 0x100000);

    mark_reserved(0x500, 0x1000);

    mark_reserved(0x7C00, 0x8400);

    extern char kernel_end;
    uint64_t kernel_end_phys = kvirt_to_phys(&kernel_end);
    mark_reserved(0x100000, KERNEL_PHYS_BASE - 0x100000);
    mark_reserved(KERNEL_PHYS_BASE, kernel_end_phys - KERNEL_PHYS_BASE);

    mark_reserved(0x10000, 0x7000);

    for (uint32_t i = 0; i < count; i++) {
        struct e820_entry *e = &MEMORY_MAP_ENTRIES[i];
        if (e->type != E820_TYPE_USABLE) continue;
        if (e->length == 0) continue;

        uint64_t start_page = (e->base + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
        uint64_t end_page = (e->base + e->length) / BUDDY_PAGE_SIZE;
        if (end_page > BUDDY_MAX_PAGES) end_page = BUDDY_MAX_PAGES;
        if (start_page >= end_page) continue;

        uint64_t run_start = 0;
        uint64_t run_end = 0;
        for (uint64_t pg = start_page; pg < end_page; pg++) {
            if (page_array[pg].flags == PAGE_ALLOCATED) {
                if (run_start == 0) {
                    run_start = pg;
                    run_end = pg + 1;
                } else {
                    run_end = pg + 1;
                }
            } else {
                if (run_start != 0) {
                    build_buddy_from_range(run_start, run_end);
                    run_start = 0;
                    run_end = 0;
                }
            }
        }
        if (run_start != 0) {
            build_buddy_from_range(run_start, run_end);
        }
    }

    kprintf("  Buddy: init complete, highest phys = 0x%x\n", highest_phys_addr);
}

void buddy_dump_stats(void) {
    kprintf("  Buddy: %d managed pages, %d free\n",
            (uint64_t)total_managed_pages, free_page_count);

    for (int o = 0; o <= BUDDY_MAX_ORDER; o++) {
        if (free_block_counts[o] > 0) {
            kprintf("    order %d (%d pages): %d blocks\n",
                    o, (1 << o), free_block_counts[o]);
        }
    }
}