#ifndef BUDDY_H
#define BUDDY_H

#include "types.h"

#define BUDDY_MAX_ORDER 11
#define BUDDY_PAGE_SIZE 4096

#define BUDDY_MAX_PAGES 131072

#define PAGE_UNAVAILABLE 0x00
#define PAGE_FREE        0x01
#define PAGE_ALLOCATED   0x02

struct page {
    uint8_t order;
    uint8_t flags;
    uint16_t _pad;
    struct page *next;
    struct page *prev;
};


_Static_assert(sizeof(struct page) == 24, "struct page must be 24 bytes");

void buddy_init(void);
void *alloc_pages(int order);
void  free_pages(void *addr, int order);
void  buddy_dump_stats(void);
void *alloc_pages(int order);
void  free_pages(void *addr, int order);


#endif