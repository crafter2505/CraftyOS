#include "kmalloc.h"
#include "slab.h"
#include "address_space.h"
#include "kprintf.h"
#include "buddy.h"

#define KMALLOC_NUM_BUCKETS 8

static const uint64_t bucket_sizes[KMALLOC_NUM_BUCKETS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

static struct kmem_cache *buckets[KMALLOC_NUM_BUCKETS];

#define KMALLOC_HEADER_SIZE 16

static int find_bucket(uint64_t total) {
    for (int i = 0; i < KMALLOC_NUM_BUCKETS; i++) {
        if (total <= bucket_sizes[i]) return i;
    }
    return -1;
}

static inline uint64_t make_slub_header(uint64_t bucket_size) {
    return (bucket_size << 1) | 0ULL;
}

static inline uint64_t make_buddy_header(int order) {
    return ((uint64_t)order << 1) | 1ULL;
}

static inline int header_is_buddy(uint64_t h) {
    return (h & 1ULL) != 0;
}

static inline uint64_t header_slub_size(uint64_t h) {
    return h >> 1;
}

static inline int header_buddy_order(uint64_t h) {
    return (int)(h >> 1);
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
        }
    }
    kprintf("  KMALLOC: %d buckets ready (16..2048 bytes)\n",
            KMALLOC_NUM_BUCKETS);
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    /* Account for the header */
    uint64_t total = size + KMALLOC_HEADER_SIZE;

    int bucket = find_bucket(total);

    if (bucket >= 0) {
        /* SLUB path */
        struct kmem_cache *c = buckets[bucket];
        void *block = kmem_cache_alloc(c);
        if (!block) return 0;

        /* Write header */
        uint64_t *hdr = (uint64_t *)block;
        *hdr = make_slub_header(bucket_sizes[bucket]);

        return (void *)((uint8_t *)block + KMALLOC_HEADER_SIZE);
    }

    /* Larger allocations use the buddy allocator. */
    uint64_t pages = (total + 4095) / 4096;
    int order = 0;
    while (((uint64_t)1 << order) < pages) order++;
    if (order > BUDDY_MAX_ORDER) return 0;

    void *block_phys = alloc_pages(order);
    if (!block_phys) return 0;

    /* alloc_pages returns a physical address. Convert to virtual. */
    void *block = (void *)(DIRECT_MAP_BASE + (uint64_t)block_phys);

    uint64_t *hdr = (uint64_t *)block;
    *hdr = make_buddy_header(order);

    return (void *)((uint8_t *)block + KMALLOC_HEADER_SIZE);
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

    /* Header sits 16 bytes before the payload. */
    uint64_t *hdr = (uint64_t *)((uint8_t *)ptr - KMALLOC_HEADER_SIZE);
    uint64_t h = *hdr;

    if (header_is_buddy(h)) {
        int order = header_buddy_order(h);
        /* Convert virtual back to physical */
        uint64_t virt = (uint64_t)hdr;
        uint64_t phys = virt - DIRECT_MAP_BASE;
        free_pages((void *)phys, order);
    } else {
        uint64_t bucket_size = header_slub_size(h);
        /* Find the bucket cache */
        for (int i = 0; i < KMALLOC_NUM_BUCKETS; i++) {
            if (bucket_sizes[i] == bucket_size) {
                kmem_cache_free(buckets[i], hdr);
                return;
            }
        }
        /* If we get here, the header was corrupted */
        kprintf("  KMALLOC: kfree of unknown block (header=0x%x)\n", h);
    }
}

void kmalloc_dump_stats(void) {
    slab_dump_stats();
}