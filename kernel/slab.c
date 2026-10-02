#include "slab.h"
#include "buddy.h"
#include "address_space.h"
#include "kprintf.h"

struct slab_metadata {
    struct slab_metadata *next_phys;
    uint64_t              reserved;
};

#define MAX_CACHES 32
static struct kmem_cache *cache_list[MAX_CACHES];
static int cache_count = 0;

struct kmem_cache *kmem_cache_create(const char *name, uint64_t object_size) {
    if (cache_count >= MAX_CACHES) return 0;
    if (object_size == 0) return 0;

    object_size = (object_size + 15) & ~15ULL;

    void *cache_mem = alloc_pages(0);
    if (!cache_mem) return 0;

    struct kmem_cache *cache =
        (struct kmem_cache *)(DIRECT_MAP_BASE + (uint64_t)cache_mem);

    int i = 0;
    while (name[i] && i < SLAB_MAX_NAME - 1) {
        cache->name[i] = name[i];
        i++;
    }
    cache->name[i] = 0;

    uint64_t usable = 4096 - sizeof(struct slab_metadata);
    uint64_t per_slab = usable / object_size;
    if (per_slab > SLAB_MAX_OBJECTS) per_slab = SLAB_MAX_OBJECTS;

    cache->object_size     = object_size;
    cache->objects_per_slab= per_slab;
    cache->total_objects   = 0;
    cache->used_objects    = 0;
    cache->free_list       = 0;
    cache->slab_list       = 0;

    cache_list[cache_count++] = cache;

    kprintf("  SLAB: created cache '%s' (obj=%d, per_slab=%d)\n",
            cache->name, cache->object_size, cache->objects_per_slab);

    return cache;
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
    (void)cache;
    return 0;
}

void kmem_cache_free(struct kmem_cache *cache, void *ptr) {
    (void)cache; (void)ptr;
}

void slab_dump_stats(void) {
    kprintf("  SLAB: %d caches\n", (uint64_t)cache_count);
    for (int i = 0; i < cache_count; i++) {
        struct kmem_cache *c = cache_list[i];
        kprintf("    '%s': obj_size=%d per_slab=%d used=%d total=%d\n",
                c->name, c->object_size, c->objects_per_slab,
                c->used_objects, c->total_objects);
    }
}