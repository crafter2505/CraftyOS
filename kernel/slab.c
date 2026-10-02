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
    if (object_size > 4096 - sizeof(struct slab_metadata)) return 0;

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

static int slab_grow(struct kmem_cache *cache) {
    void *page_phys = alloc_pages(0);
    if (!page_phys) return -1;

    uint8_t *page = (uint8_t *)phys_to_virt((uint64_t)page_phys);
    struct slab_metadata *meta = (struct slab_metadata *)page;
    meta->next_phys = (struct slab_metadata *)cache->slab_list;
    meta->reserved = 0;
    cache->slab_list = page;

    uint8_t *obj_start = page + sizeof(struct slab_metadata);
    for (uint64_t i = 0; i < cache->objects_per_slab; i++) {
        uint8_t *obj = obj_start + i * cache->object_size;
        *(void **)obj = cache->free_list;
        cache->free_list = obj;
    }

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

    uint64_t offset = (uint64_t)ptr;
    int belongs_to_cache = 0;
    for (uint8_t *page = (uint8_t *)cache->slab_list;
         page;
         page = (uint8_t *)((struct slab_metadata *)page)->next_phys) {
        uint64_t obj_start = (uint64_t)page + sizeof(struct slab_metadata);
        uint64_t obj_end = obj_start +
            cache->objects_per_slab * cache->object_size;
        if (offset >= obj_start && offset < obj_end &&
            (offset - obj_start) % cache->object_size == 0) {
            belongs_to_cache = 1;
            break;
        }
    }
    if (!belongs_to_cache) return;

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