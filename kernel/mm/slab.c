#include "slab.h"
#include "buddy.h"
#include "address_space.h"
#include "kprintf.h"

#define MAX_CACHES 32
static struct kmem_cache *cache_list[MAX_CACHES];
static int cache_count = 0;

struct kmem_cache *kmem_cache_create(const char *name, uint64_t object_size) {
    if (cache_count >= MAX_CACHES) return 0;
    if (object_size == 0) return 0;
    if (object_size > 4096) return 0;

    object_size = (object_size + 15) & ~15ULL;
    if (object_size > 4096) return 0;

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

    uint64_t per_slab = 4096 / object_size;
    if (per_slab > SLAB_MAX_OBJECTS) per_slab = SLAB_MAX_OBJECTS;

    cache->object_size     = object_size;
    cache->objects_per_slab= per_slab;
    cache->total_objects   = 0;
    cache->used_objects    = 0;
    cache->free_list       = 0;
    cache->slab_list       = 0;
    cache->bucket_index    = -1;

    cache_list[cache_count++] = cache;

    kprintf("  SLAB: created cache '%s' (obj=%d, per_slab=%d)\n",
            cache->name, cache->object_size, cache->objects_per_slab);

    return cache;
}

static int slab_grow(struct kmem_cache *cache) {
    void *page_phys = alloc_pages(0);
    if (!page_phys) return -1;

    uint64_t page_virt = DIRECT_MAP_BASE + (uint64_t)page_phys;
    struct page *pg = page_from_phys((uint64_t)page_phys);
    if (!pg) {
        free_pages(page_phys, 0);
        return -1;
    }
    pg->flags = PAGE_FLAG_SLAB;
    pg->cache = cache;
    pg->order = 0;

    uint8_t *obj_start = (uint8_t *)page_virt;
    for (uint64_t i = 0; i < cache->objects_per_slab; i++) {
        uint8_t *obj = obj_start + i * cache->object_size;
        *(void **)obj = cache->free_list;
        cache->free_list = obj;
    }

    pg->next = (struct page *)cache->slab_list;
    cache->slab_list = (void *)pg;

    cache->total_objects += cache->objects_per_slab;

    kprintf("  SLAB: grew '%s' by %d objects (total=%d)\n",
            cache->name, cache->objects_per_slab, cache->total_objects);
    return 0;
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
    if (!cache) return 0;

    if (!cache->free_list && slab_grow(cache) != 0) return 0;

    void *obj = cache->free_list;
    cache->free_list = *(void **)obj;
    cache->used_objects++;
    return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *ptr) {
    if (!cache || !ptr || cache->used_objects == 0) return;

    struct page *pg = page_from_virt((uint64_t)ptr);
    if (!pg || pg->flags != PAGE_FLAG_SLAB || pg->cache != cache) return;

    uint64_t page_virt = page_to_virt(pg);
    uint64_t offset = (uint64_t)ptr - page_virt;
    if (offset >= cache->objects_per_slab * cache->object_size ||
        offset % cache->object_size != 0) return;

    for (void *free_obj = cache->free_list;
         free_obj;
         free_obj = *(void **)free_obj) {
        if (free_obj == ptr) return;
    }

    *(void **)ptr = cache->free_list;
    cache->free_list = ptr;
    cache->used_objects--;
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