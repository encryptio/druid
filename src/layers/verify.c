#include "layers/verify.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <err.h>

#include "endian-fns.h"
#include "crc.h"
#include "logger.h"

/*
 * verification layer, using crc32
 *
 * there is no header information, the all-zero device is valid.
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
 * all crcs are xored with the crc of the zero block. this makes the all-zero
 * device valid.
 *
 */

struct verify_io {
    struct bdev *base;

    uint64_t hashes_per_block;
    uint64_t hash_block_count;
    uint64_t data_block_count;

    uint8_t *hash_block;
    uint64_t which_hash_block;

    uint32_t zero_crc;
};

static inline void create_zero_crc(struct bdev *dev) {
    struct verify_io *io = dev->m;

    uint8_t zeroes[16];
    memset(zeroes, 0, 16);

    int to_send = dev->block_size;
    uint32_t crc = crc_init();

    while ( to_send >= 16 ) {
        to_send -= 16;
        crc = crc_update(crc, zeroes, 16);
    }

    if ( to_send )
        crc = crc_update(crc, zeroes, to_send);

    io->zero_crc = crc_finalize(crc);
}

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

    uint32_t needed_crc = unpack_be32(io->hash_block+interior_offset*4) ^ io->zero_crc;

    if ( !io->base->read_block(io->base, needed_data_block, into) )
        return false;

    uint32_t read_crc = calc_crc32(into, self->block_size);

    if ( needed_crc != read_crc ) {
        logger(LOG_JUNK, "verify", "CRC error on block %llu (mapped %llu) - %d != %d",
                (unsigned long long)needed_data_block,
                (unsigned long long)which,
                read_crc,
                needed_crc);
        return false;
    }

    return true;
}

static bool verify_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    struct verify_io *io = self->m;
    assert(which < self->block_count);

    uint64_t needed_chunk = which/io->hashes_per_block;
    uint64_t needed_hash_block = needed_chunk * (io->hashes_per_block+1);
    uint64_t needed_data_block = which+needed_chunk+1;
    uint64_t interior_offset = which % io->hashes_per_block;

    if ( io->which_hash_block != needed_hash_block ) {
        if ( !io->base->read_block(io->base, needed_hash_block, io->hash_block) ) {
            // hash block failed to read, assume it's zeroed - we won't lose
            // any more data than we've already lost by doing this, and we
            // continue to write to the drive

            memset(io->hash_block, 0, self->block_size);
        }
        io->which_hash_block = needed_hash_block;
    }
    
    // write the crc
    // TODO: don't write the crc header until chunk change or flushed
    pack_be32(calc_crc32(from, self->block_size) ^ io->zero_crc, io->hash_block+interior_offset*4);
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

static void verify_sync(struct bdev *self) {
    struct verify_io *io = self->m;
    io->base->sync(io->base);
}

struct bdev *verify_create(struct bdev *base) {
    struct bdev *dev;

    if ( base->block_count == 1 ) {
        logger(LOG_ERR, "verify", "Can't create a verify object atop a one-block device");
        return NULL;
    }

    if ( base->block_size < 4 ) {
        logger(LOG_ERR, "verify", "Can't create a verify object atop a device with a block size less than 4 bytes (is %llu bytes)",
                (unsigned long long)base->block_size);
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
    dev->sync         = verify_sync;
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

    create_zero_crc(dev);

    snprintf(dev->name, BDEV_NAME_LEN, "verify");

    return dev;
}

