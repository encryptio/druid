#include "layers/verify.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <err.h>

#include "endian-fns.h"
#include "crc.h"

/*
 * verification layer, using crc32
 *
 * inserts hash blocks in intervals in the device below it,
 * verifies when reading and updates when writing.
 *
 * for a 16 block disk with 8 hashes per block (low)
 * disk layout (blocks):
 *   HddddddddHddddddddHddddd
 *
 * each /Hd+/ is called a "chunk"
 *
 * hash blocks have the following structure:
 *   32-bit crc, big-endian, for data block 1 in the chunk
 *   32-bit crc, big-endian, for data block 2 in the chunk
 *   32-bit crc, big-endian, for data block 3 in the chunk
 *   ...
 *
 */

struct verify_io {
    struct bdev *base;

    uint64_t hashes_per_block;
    uint64_t hash_block_count;
    uint64_t data_block_count;

    uint8_t *hash_block;
    uint64_t which_hash_block;
};

static inline bool verify_is_hblock(struct bdev *dev, uint64_t index) {
    struct verify_io *io = dev->m;
    return (index % (io->hashes_per_block+1) == 0);
}

static bool verify_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    struct verify_io *io = self->m;
    assert(which < self->block_count);

    uint64_t needed_chunk = which/io->hashes_per_block;
    uint64_t needed_hash_block = needed_chunk * (io->hashes_per_block+1);
    uint64_t needed_data_block = which+needed_chunk+1;
    uint64_t interior_offset = which % io->hashes_per_block;

    if ( io->which_hash_block != needed_hash_block ) {
        if ( !io->base->read_block(io->base, needed_hash_block, io->hash_block) )
            return false;
        io->which_hash_block = needed_hash_block;
    }

    uint32_t needed_crc = unpack_be32(io->hash_block+interior_offset*4);

    if ( !io->base->read_block(io->base, needed_data_block, into) )
        return false;

    uint32_t read_crc = calc_crc32(into, self->block_size);

    if ( needed_crc != read_crc ) {
        //fprintf(stderr, "[verify] CRC error on block %llu (mapped %llu) - %d != %d\n", (unsigned long long)needed_data_block, (unsigned long long)which, read_crc, needed_crc);
        return false;
    }

    return true;
}

static bool verify_write_block(struct bdev *self, uint64_t which, uint8_t *from) {
    struct verify_io *io = self->m;
    assert(which < self->block_count);

    uint64_t needed_chunk = which/io->hashes_per_block;
    uint64_t needed_hash_block = needed_chunk * (io->hashes_per_block+1);
    uint64_t needed_data_block = which+needed_chunk+1;
    uint64_t interior_offset = which % io->hashes_per_block;

    if ( io->which_hash_block != needed_hash_block ) {
        if ( !io->base->read_block(io->base, needed_hash_block, io->hash_block) )
            return false;
        io->which_hash_block = needed_hash_block;
    }
    
    // write the crc
    // TODO: don't write the crc header until chunk change or flushed
    pack_be32(calc_crc32(from, self->block_size), io->hash_block+interior_offset*4);
    if ( !io->base->write_block(io->base, needed_hash_block, io->hash_block) )
        return false;

    // then write the data
    if ( !io->base->write_block(io->base, needed_data_block, from) )
        return false;

    return true;
}

static void verify_close(struct bdev *self) {
    struct verify_io *io = self->m;
    free(io->hash_block);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void verify_flush(struct bdev *self) {
    struct verify_io *io = self->m;
    io->base->flush(io->base);
}

static void verify_clear_caches(struct bdev *self) {
    struct verify_io *io = self->m;
    io->which_hash_block = -1; // overflow
    io->base->clear_caches(io->base);
}

struct bdev *verify_create(struct bdev *base) {
    struct bdev *dev;

    if ( base->block_count == 1 ) {
        fprintf(stderr, "[verify] can't create a verify object atop a one-block device");
        return NULL;
    }

    if ( base->block_size < 2 ) {
        fprintf(stderr, "[verify] can't create a verify object atop a device with a block size less than 2 bytes");
        return NULL;
    }

    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct verify_io))) == NULL )
        err(1, "Couldn't allocate space for bdev:verify_io");
    struct verify_io *io = dev->m;

    io->base = base;
    dev->block_size = base->block_size;
    
    // accessor functions
    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;
    dev->read_block   = verify_read_block;
    dev->write_block  = verify_write_block;
    dev->close        = verify_close;
    dev->flush        = verify_flush;
    dev->clear_caches = verify_clear_caches;

    if ( (dev->generic_block_buffer = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for bdev:verify_io:generic_block_buffer");

    if ( (io->hash_block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for bdev:verify_io:hash_block");
    io->which_hash_block = -1; // overflows

    // calculate the constants needed for this base device
    io->hashes_per_block = dev->block_size/4; // 4 bytes per hash
    io->hash_block_count = (base->block_count + io->hashes_per_block) / (1+io->hashes_per_block);
    io->data_block_count = base->block_count - io->hash_block_count;

    if ( verify_is_hblock(dev, io->base->block_count-1) ) {
        // trailing hash block - it represents no data, and is thus a lost block
        // note that hash_block_count + data_block_count is not base->block_count
        io->hash_block_count--;
    }

    assert(io->hashes_per_block > 0);
    assert(io->hash_block_count > 0);
    assert(io->data_block_count > 0);

    dev->block_count = io->data_block_count;

    return dev;
}

