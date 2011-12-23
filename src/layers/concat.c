#include "layers/concat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <err.h>

#include "logger.h"

// TODO: testing routines

struct concat_io {
    struct bdev **devices;
    int count;

    int last;
    uint64_t last_offset;
    uint64_t last_len;
};

static inline bool concat_find(struct concat_io *io, uint64_t block) {
    // already found
    if ( io->last_offset <= block && block < io->last_offset+io->last_len )
        return true;

    uint64_t offset = 0;
    for (int i = 0; i < io->count; i++)
        if ( io->devices[i]->block_count > block ) {
            io->last = i;
            io->last_offset = offset;
            io->last_len = io->devices[i]->block_count;
            return true;
        } else {
            block  -= io->devices[i]->block_count;
            offset += io->devices[i]->block_count;
        }
    return false;
}

static bool concat_read_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into) {
    // TODO: pass through the request if it falls in a single device
    return generic_read_bytes(self, start, len, into);
}

static bool concat_write_bytes(struct bdev *self, uint64_t start, uint64_t len, const uint8_t *from) {
    // TODO: pass through the request if it falls in a single device
    return generic_write_bytes(self, start, len, from);
}

static bool concat_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    struct concat_io *io = self->m;
    assert(which < self->block_count);
    assert(concat_find(io, which));
    return io->devices[io->last]->read_block(io->devices[io->last], which-io->last_offset, into);
}

static bool concat_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    struct concat_io *io = self->m;
    assert(which < self->block_count);
    assert(concat_find(io, which));
    return io->devices[io->last]->write_block(io->devices[io->last], which-io->last_offset, from);
}

static void concat_close(struct bdev *self) {
    struct concat_io *io = self->m;
    free(io->devices);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void concat_clear_caches(struct bdev *self) {
    struct concat_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->clear_caches(io->devices[i]);
}

static void concat_flush(struct bdev *self) {
    struct concat_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->flush(io->devices[i]);
}

static void concat_sync(struct bdev *self) {
    concat_flush(self);

    struct concat_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->sync(io->devices[i]);
}

struct bdev *concat_open(struct bdev **devices, int count) {
    assert(count > 0);

    /*
     * unfortunately, this fails because the close method is mishandled

    // no need to concat a single device
    if ( count == 1 )
        return devices[0];
    */

    // make sure all the block sizes are identical
    uint64_t block_size = devices[0]->block_size;
    for (int i = 1; i < count; i++) {
        assert(devices[i]);

        if ( devices[i]->block_size != block_size ) {
            logger(LOG_ERR, "concat", "Can't concat devices with"
                    "different block sizes (%llu and %llu)",
                    block_size,
                    devices[i]->block_size);
            return NULL;
        }
    }

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't malloc space for concat:bdev");

    struct concat_io *io;
    if ( (io = malloc(sizeof(struct concat_io))) == NULL )
        err(1, "Couldn't malloc space for concat:io");

    if ( (io->devices = malloc(sizeof(struct bdev *)*count)) == NULL )
        err(1, "Couldn't malloc space for concat:io:devices");

    // we fall back to generic_*_bytes if the operation is not in a single device
    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't malloc space for concat:generic_block_buffer");

    io->count = count;
    memcpy(io->devices, devices, sizeof(struct bdev *)*count);
    io->last = 0;
    io->last_offset = 0;
    io->last_len = devices[0]->block_count;

    dev->m           = io;
    dev->block_size  = block_size;
    dev->block_count = 0;
    for (int i = 0; i < count; i++)
        dev->block_count += devices[i]->block_count;

    dev->read_bytes   = concat_read_bytes;
    dev->write_bytes  = concat_write_bytes;
    dev->read_block   = concat_read_block;
    dev->write_block  = concat_write_block;
    dev->close        = concat_close;
    dev->clear_caches = concat_clear_caches;
    dev->flush        = concat_flush;
    dev->sync         = concat_sync;

    snprintf(dev->name, BDEV_NAME_LEN, "concat");

    return dev;
    
}

