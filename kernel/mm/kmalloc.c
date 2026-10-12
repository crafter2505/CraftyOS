#include "kmalloc.h"
#include "slab.h"
#include "address_space.h"
#include "kprintf.h"
#include "buddy.h"
#include "page.h"

#define KMALLOC_NUM_BUCKETS 8

static const uint64_t bucket_sizes[KMALLOC_NUM_BUCKETS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

static struct kmem_cache *buckets[KMALLOC_NUM_BUCKETS];

static int find_bucket(uint64_t size) {
    for (int i = 0; i < KMALLOC_NUM_BUCKETS; i++) {
        if (size <= bucket_sizes[i]) return i;
    }
    return -1;
}

void kmalloc_init(void) {
    for (int i = 0; i < KMALLOC_NUM_BUCKETS; i++) {
        char name[16];
        const char *prefix = "kmalloc-";
        int p = 0;
        while (prefix[p]) { name[p] = prefix[p]; p++; }

        uint64_t s = bucket_sizes[i];
        char tmp[8];
        int t = 0;
        if (s == 0) { tmp[t++] = '0'; }
        while (s > 0) { tmp[t++] = '0' + (s % 10); s /= 10; }
        while (t > 0) { name[p++] = tmp[--t]; }
        name[p] = 0;

        buckets[i] = kmem_cache_create(name, bucket_sizes[i]);
        if (!buckets[i]) {
            kprintf("  KMALLOC: failed to create cache %s\n", name);
        } else {
            buckets[i]->bucket_index = i;
        }
    }
    kprintf("  KMALLOC: %d buckets ready (16..2048 bytes)\n",
            KMALLOC_NUM_BUCKETS);
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    int bucket = find_bucket(size);

    if (bucket >= 0) {
        /* SLUB path */
        struct kmem_cache *c = buckets[bucket];
        return kmem_cache_alloc(c);
    }

    /* Larger allocations use the buddy allocator. */
    uint64_t pages = (size + 4095) / 4096;
    int order = 0;
    while (((uint64_t)1 << order) < pages) order++;
    if (order > BUDDY_MAX_ORDER) return 0;

    void *block_phys = alloc_pages(order);
    if (!block_phys) return 0;

    /* alloc_pages returns a physical address. Convert to virtual. */
    void *block = (void *)(DIRECT_MAP_BASE + (uint64_t)block_phys);

    return block;
}

void *kzalloc(size_t size) {
    void *p = kmalloc(size);
    if (!p) return 0;
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < size; i++) b[i] = 0;
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct page *pg = page_from_virt((uint64_t)ptr);
    if (!pg) return;

    if (pg->flags == PAGE_FLAG_SLAB) {
        kmem_cache_free(pg->cache, ptr);
    } else if (pg->flags == PAGE_FLAG_BUDDY_ALLOC) {
        int order = pg->order;
        uint64_t phys = (uint64_t)ptr - DIRECT_MAP_BASE;
        phys &= ~4095ULL;
        free_pages((void *)phys, order);
    } else {
        kprintf("  KMALLOC: kfree of unmanaged page at 0x%x (flags=0x%x)\n",
                (uint64_t)ptr, pg->flags);
    }
}

void kmalloc_dump_stats(void) {
    slab_dump_stats();
}