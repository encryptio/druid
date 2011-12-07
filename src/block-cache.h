#ifndef __BLOCK_CACHE_H__
#define __BLOCK_CACHE_H__

#include <inttypes.h>

#include "bdev.h"

struct block_cache *bcache_create(struct bdev *base, uint32_t limit);
void bcache_destroy(struct block_cache *bc);

// Note that the pointer returned by read_block should not be modified unless you
// immediately call write_block on it afterwards
uint8_t *bcache_read_block(struct block_cache *bc, uint64_t which);
void bcache_write_block(struct block_cache *bc, uint64_t which, uint8_t *from);
void bcache_clear(struct block_cache *bc);
void bcache_flush(struct block_cache *bc);

#endif
