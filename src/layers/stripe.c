#include "layers/stripe.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <err.h>

// TODO: testing routines

struct stripe_io {
    struct bdev **devices;
    int count;
};

static bool stripe_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    struct stripe_io *io = self->m;
    assert(which < self->block_count);

    int device     = which % io->count;
    uint64_t block = which / io->count;

    return io->devices[device]->read_block(io->devices[device], block, into);
}

static bool stripe_write_block(struct bdev *self, uint64_t which, uint8_t *from) {
    struct stripe_io *io = self->m;
    assert(which < self->block_count);

    int device     = which % io->count;
    uint64_t block = which / io->count;

    return io->devices[device]->write_block(io->devices[device], block, from);
}

static void stripe_close(struct bdev *self) {
    struct stripe_io *io = self->m;
    free(io->devices);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void stripe_clear_caches(struct bdev *self) {
    struct stripe_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->clear_caches(io->devices[i]);
}

static void stripe_flush(struct bdev *self) {
    struct stripe_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->flush(io->devices[i]);
}

static void stripe_sync(struct bdev *self) {
    stripe_flush(self);

    struct stripe_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->sync(io->devices[i]);
}

struct bdev *stripe_open(struct bdev **devices, int count) {
    assert(count > 0);

    // TODO: figure out how to handle closing a noop device,
    //       and use it here when count == 1

    // make sure all the block sizes are identical
    uint64_t block_size = devices[0]->block_size;
    for (int i = 1; i < count; i++) {
        assert(devices[i]);

        if ( devices[i]->block_size != block_size ) {
            fprintf(stderr, "[stripe] can't stripe devices with"
                    "different block sizes (%llu and %llu)\n",
                    block_size,
                    devices[i]->block_size);
            return NULL;
        }
    }

    uint64_t min_drive_size = devices[0]->block_count;
    uint64_t max_drive_size = devices[0]->block_count;
    for (int i = 1; i < count; i++) {
        if ( devices[i]->block_count < min_drive_size )
            min_drive_size = devices[i]->block_count;
        if ( devices[i]->block_count > max_drive_size )
            max_drive_size = devices[i]->block_count;
    }

    if ( min_drive_size != max_drive_size )
        fprintf(stderr, "[stripe] some disks in array are smaller than others. "
                "will truncate long drives to %llu blocks. "
                "(longest = %llu blocks)\n",
                min_drive_size, max_drive_size);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't malloc space for stripe:bdev");

    struct stripe_io *io;
    if ( (io = malloc(sizeof(struct stripe_io))) == NULL )
        err(1, "Couldn't malloc space for stripe:io");

    if ( (io->devices = malloc(sizeof(struct bdev *)*count)) == NULL )
        err(1, "Couldn't malloc space for stripe:io:devices");

    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't malloc space for stripe:generic_block_buffer");

    io->count = count;
    memcpy(io->devices, devices, sizeof(struct bdev *)*count);

    dev->m           = io;
    dev->block_size  = block_size;
    dev->block_count = min_drive_size * count;

    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;

    dev->read_block   = stripe_read_block;
    dev->write_block  = stripe_write_block;
    dev->close        = stripe_close;
    dev->clear_caches = stripe_clear_caches;
    dev->flush        = stripe_flush;
    dev->sync         = stripe_sync;

    return dev;
}

