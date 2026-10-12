#ifndef SLAB_H
#define SLAB_H

#include "types.h"

#define SLAB_MAX_NAME 32
#define SLAB_MAX_OBJECTS 256

struct kmem_cache {
    char     name[SLAB_MAX_NAME];
    uint64_t object_size;
    uint64_t objects_per_slab;
    uint64_t total_objects;
    uint64_t used_objects;
    void    *free_list;
    void    *slab_list;
    /* Index into buckets[], or -1 for user-created caches. */
    int      bucket_index;
};

struct kmem_cache *kmem_cache_create(const char *name, uint64_t object_size);
void              *kmem_cache_alloc(struct kmem_cache *cache);
void               kmem_cache_free(struct kmem_cache *cache, void *ptr);
void               slab_dump_stats(void);

#endif