#ifndef BUDDY_H
#define BUDDY_H

#include "types.h"
#include "page.h"   /* struct page now lives here */

#define BUDDY_MAX_ORDER     11
#define BUDDY_PAGE_SIZE     4096
#define BUDDY_MAX_PAGES     131072

void buddy_init(void);
void *alloc_pages(int order);
void  free_pages(void *addr, int order);
void  buddy_dump_stats(void);

#endif