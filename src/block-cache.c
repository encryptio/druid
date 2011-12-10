#include "block-cache.h"

#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include <string.h>
#include <stdio.h>

#include "bitvector.h"

// TODO: make this thrash less on hash collisions

struct block_cache {
    struct bdev *base;
    uint32_t  size; uint8_t  *data; uint64_t *indexes;
    uint8_t  *dirty;
};

static inline uint32_t hash_u64(uint64_t key) {
    // from http://www.concentric.net/~ttwang/tech/inthash.htm
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t) key;
}

struct block_cache *bcache_create(struct bdev *base, uint32_t limit) {
    assert(base != NULL);
    assert(limit >= 1);

    struct block_cache *bc;
    if ( (bc = malloc(sizeof(struct block_cache))) == NULL )
        err(1, "Couldn't allocate space for block_cache structure");

    bc->base = base;
    bc->size = limit;

    if ( (bc->data = malloc(base->block_size * limit)) == NULL )
        err(1, "Couldn't allocate space for block_cache:data "
                "(%llu blocks of %llu bytes each)",
                (unsigned long long)limit,
                (unsigned long long)base->block_size);

    if ( (bc->indexes = malloc(8 * limit)) == NULL )
        err(1, "Couldn't allocate space for block_cache:indexes");
    memset(bc->indexes, 0xFF, 8*limit);

    if ( (bc->dirty = malloc((limit+7)/8)) == NULL )
        err(1, "Couldn't allocate space for block_cache:dirty");
    memset(bc->dirty, 0x00, (limit+7)/8);

    return bc;
}

void bcache_destroy(struct block_cache *bc) {
    bcache_flush(bc);

    free(bc->data);
    free(bc->indexes);
    free(bc->dirty);
    free(bc);
}

static void bcache_evict(struct block_cache *bc, uint32_t hv) {
    size_t offset = hv*bc->base->block_size;
    if ( bit_get(bc->dirty, hv) )
        if ( !bc->base->write_block(bc->base, bc->indexes[hv], bc->data + offset) )
            fprintf(stderr, "[block-cache] FAILED TO WRITE BLOCK WHEN EVICTING FROM CACHE\n");

    bit_clear(bc->dirty, hv);
    bc->indexes[hv] = 0xFFFFFFFFFFFFFFFFULL;
}

uint8_t *bcache_read_block(struct block_cache *bc, uint64_t which) {
    uint32_t hv = hash_u64(which) % bc->size;
    size_t offset = hv*bc->base->block_size;

    if ( bc->indexes[hv] == which )
        return bc->data + offset;

    bcache_evict(bc, hv);

    if ( !bc->base->read_block(bc->base, which, bc->data + offset) )
        return NULL;

    bc->indexes[hv] = which;

    return bc->data + offset;
}

void bcache_write_block(struct block_cache *bc, uint64_t which, uint8_t *from) {
    uint32_t hv = hash_u64(which) % bc->size;
    size_t offset = hv*bc->base->block_size;

    if ( bc->indexes[hv] != which )
        bcache_evict(bc, hv);

    if ( from != bc->data + offset )
        memcpy(bc->data + offset, from, bc->base->block_size);
    bit_set(bc->dirty, hv);
}

void bcache_clear(struct block_cache *bc) {
    bcache_flush(bc);
    memset(bc->indexes, 0xFF, 8*bc->size);
}

void bcache_flush(struct block_cache *bc) {
    for (int i = 0; i < bc->size; i++)
        bcache_evict(bc, i);
}

