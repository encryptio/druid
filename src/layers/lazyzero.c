#include "layers/lazyzero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "endian-fns.h"
#include "bitvector.h"
#include "block-cache.h"
#include "logger.h"

/*
 * lazy zeroing of devices by way of a header bitmap
 *
 * disk format:
 *     header block
 *     bitmap
 *     data
 *
 * header block format:
 *     magic number "LAZY0000"
 *     uint64_t device total block count
 *     uint64_t number of bitmap blocks
 *     uint64_t chunk size
 *
 * the data is divided into chunks, contiguously throughout the disk.
 * note that the last chunk (and only the last chunk) may be short.
 *
 * bitmap is a packed bit vector, with zeroes representing the chunks that
 * have not been initialized yet, and ones representing the chunks that have
 * been initialized.
 */

#define MAGIC "LAZY0000"

struct lazy_io {
    struct bdev *base;

    uint64_t bits_per_block;
    uint64_t bitmap_blocks;
    uint64_t chunk_size;

    struct block_cache *cache;
};

static bool lazyzero_chunk_usable(struct bdev *self, uint64_t chunk) {
    struct lazy_io *io = self->m;
    assert(chunk < (self->block_count + io->chunk_size - 1)/io->chunk_size);

    uint64_t bitmap_block = chunk / io->bits_per_block;
    uint64_t interior     = chunk % io->bits_per_block;

    uint8_t *bl = bcache_read_block(io->cache, bitmap_block);
    if ( !bl ) return false; // TODO: what to do, false or true, what to do

    return bit_get(bl, interior);
}

static bool lazyzero_set_chunk_usable(struct bdev *self, uint64_t chunk) {
    struct lazy_io *io = self->m;
    assert(chunk < (self->block_count + io->chunk_size - 1)/io->chunk_size);

    uint64_t bitmap_block = chunk / io->bits_per_block;
    uint64_t interior     = chunk % io->bits_per_block;

    uint8_t *bl = bcache_read_block(io->cache, bitmap_block);
    if ( !bl ) return false;

    bit_set(bl, interior);

    bcache_write_block(io->cache, bitmap_block, bl);

    return true;
}

static bool lazyzero_clear_chunk(struct bdev *self, uint64_t chunk) {
    struct lazy_io *io = self->m;

    memset(self->generic_block_buffer, 0, self->block_size);

    uint64_t base_block = chunk * io->chunk_size + io->bitmap_blocks + 1;
    for (uint64_t i = 0; i < io->chunk_size; i++)
        if ( base_block+i < io->base->block_count )
            if ( !io->base->write_block(io->base, base_block + i, self->generic_block_buffer) )
                return false;

    return true;
}

static bool lazyzero_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    assert(which < self->block_count);
    struct lazy_io *io = self->m;

    uint64_t chunk = which / io->chunk_size;
    if ( !lazyzero_chunk_usable(self, chunk) ) {
        memset(into, 0, self->block_size);
        return true;
    }

    return io->base->read_block(io->base, which + 1 + io->bitmap_blocks, into);
}

static bool lazyzero_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    assert(which < self->block_count);
    struct lazy_io *io = self->m;

    uint64_t chunk = which / io->chunk_size;
    if ( !lazyzero_chunk_usable(self, chunk) ) {
        if ( !lazyzero_clear_chunk(self, chunk) )
            return false;
        if ( !lazyzero_set_chunk_usable(self, chunk) )
            return false;
    }

    return io->base->write_block(io->base, which + 1 + io->bitmap_blocks, from);
}

static void lazyzero_close(struct bdev *self) {
    struct lazy_io *io = self->m;
    bcache_destroy(io->cache);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void lazyzero_clear_caches(struct bdev *self) {
    struct lazy_io *io = self->m;
    bcache_clear(io->cache);
    io->base->clear_caches(io->base);
}

static void lazyzero_flush(struct bdev *self) {
    struct lazy_io *io = self->m;
    bcache_flush(io->cache);
    io->base->flush(io->base);
}

static void lazyzero_sync(struct bdev *self) {
    struct lazy_io *io = self->m;
    bcache_flush(io->cache);
    io->base->sync(io->base);
}

bool lazyzero_create(struct bdev *base) {
    if ( base->block_size < 32 ) {
        logger(LOG_ERR, "lazyzero", "Can't create a lazyzero on a device with a block size less than 32 bytes");
        return false;
    }
    if ( base->block_count < 3 ) {
        logger(LOG_ERR, "lazyzero", "Can't create a lazyzero on a device with less than 3 blocks");
        return false;
    }

    uint64_t bits_per_block = base->block_size*8;
    uint64_t chunk_size = 1024; // TODO: automatically adjust based on device size. this is okay for 2TB @ 512B
    uint64_t bitmap_bits = ((base->block_count-1) + chunk_size - 1) / chunk_size;
    uint64_t bitmap_blocks = (bitmap_bits + bits_per_block - 1) / bits_per_block;

    assert(bitmap_blocks * bits_per_block >= base->block_count-1-bitmap_blocks);

    uint8_t *header;
    if ( (header = malloc(base->block_size)) == NULL )
        err(1, "Couldn't allocate space for block");
    memset(header, 0, base->block_size);

    memcpy(header, MAGIC, 8);
    pack_be64(base->block_count, header+8);
    pack_be64(bitmap_blocks, header+16);
    pack_be64(chunk_size, header+24);

    bool ret;
    if ( !(ret = base->write_block(base, 0, header)) )
        goto END;

    // now write the bitmap blocks
    memset(header, 0, base->block_size);

    for (uint64_t i = 1; i < bitmap_blocks+1; i++)
        if ( !(ret = base->write_block(base, i+1, header)) )
            goto END;

END:
    free(header);

    return ret;
}

struct bdev *lazyzero_open(struct bdev *base) {
    if ( base->block_size < 32 ) {
        logger(LOG_ERR, "lazyzero", "Can't create a lazyzero on a device with a block size less than 32 bytes");
        return false;
    }
    if ( base->block_count < 3 ) {
        logger(LOG_ERR, "lazyzero", "Can't create a lazyzero on a device with less than 3 blocks");
        return false;
    }

    uint8_t *buf;
    if ( (buf = malloc(base->block_size)) == NULL )
        err(1, "Couldn't allocate space for block");

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for lazyzero:bdev");

    struct lazy_io *io;
    if ( (io = malloc(sizeof(struct lazy_io))) == NULL )
        err(1, "Couldn't allocate space for lazyzero:io");

    if ( !base->read_block(base, 0, buf) )
        goto BAD_END;

    if ( memcmp(buf, MAGIC, 8) != 0 ) {
        logger(LOG_ERR, "lazyzero", "Bad magic number");
        goto BAD_END;
    }

    uint64_t header_block_count = unpack_be64(buf+8);
    io->bitmap_blocks  = unpack_be64(buf+16);
    io->chunk_size     = unpack_be64(buf+24);
    io->bits_per_block = base->block_size*8;
    io->base           = base;

    if ( header_block_count != base->block_count ) {
        logger(LOG_ERR, "lazyzero", "Device was initialized for %llu blocks, "
                "but is now %llu blocks.",
                header_block_count,
                base->block_count);
        goto BAD_END;
    }

    if ( io->bitmap_blocks * io->bits_per_block < base->block_count-1-io->bitmap_blocks ) {
        logger(LOG_ERR, "lazyzero", "Not enough bitmap blocks for this device size");
        goto BAD_END;
    }

    dev->m = io;
    dev->generic_block_buffer = buf;

    dev->block_size = base->block_size;
    dev->block_count = base->block_count - 1 - io->bitmap_blocks;
    
    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;
    dev->read_block   = lazyzero_read_block;
    dev->write_block  = lazyzero_write_block;
    dev->close        = lazyzero_close;
    dev->clear_caches = lazyzero_clear_caches;
    dev->flush        = lazyzero_flush;
    dev->sync         = lazyzero_sync;

    io->cache = bcache_create(base, 16);

    return dev;

BAD_END:
    free(buf);
    free(dev);
    free(io);
    return NULL;
}

