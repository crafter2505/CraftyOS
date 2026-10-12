#ifndef PAGE_H
#define PAGE_H
#include "types.h"

struct kmem_cache;

#define PAGE_FLAG_UNAVAILABLE  (1U << 0)
#define PAGE_FLAG_BUDDY_FREE   (1U << 1)
#define PAGE_FLAG_BUDDY_ALLOC  (1U << 2)
#define PAGE_FLAG_SLAB         (1U << 3)

struct page {
    uint8_t order;
    uint8_t flags;
    uint16_t _pad;
    struct kmem_cache *cache;
    struct page *next;
    struct page *prev;
};

_Static_assert(sizeof(struct page) % 8 == 0, "struct page must be 8-byte aligned");

struct page *page_from_phys(uint64_t phys);
struct page *page_from_virt(uint64_t virt);
uint64_t page_to_phys(struct page *p);
uint64_t page_to_virt(struct page *p);
uint64_t page_to_base_virt(struct page *p);


#endif