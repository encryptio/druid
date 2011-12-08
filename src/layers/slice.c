#include "layers/slice.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <err.h>

struct slice_io {
    struct bdev *base;
    uint64_t start, len;
};

static bool slice_read_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into) {
    struct slice_io *io = self->m;
    assert(start+len <= io->len*self->block_size);
    return io->base->read_bytes(io->base, start + self->block_size*io->start, len, into);
}

static bool slice_write_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into) {
    struct slice_io *io = self->m;
    assert(start+len <= io->len*self->block_size);
    return io->base->write_bytes(io->base, start + self->block_size*io->start, len, into);
}

static bool slice_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    struct slice_io *io = self->m;
    assert(which < io->len);
    return io->base->read_block(io->base, which+io->start, into);
}

static bool slice_write_block(struct bdev *self, uint64_t which, uint8_t *from) {
    struct slice_io *io = self->m;
    assert(which < io->len);
    return io->base->write_block(io->base, which+io->start, from);
}

static void slice_close(struct bdev *self) {
    free(self->m);
    free(self);
}

static void slice_clear_caches(struct bdev *self) {
    struct slice_io *io = self->m;
    io->base->clear_caches(io->base);
}

static void slice_flush(struct bdev *self) {
    struct slice_io *io = self->m;
    io->base->flush(io->base);
}

struct bdev *slice_open(struct bdev *base, uint64_t start, uint64_t len) {
    assert(len > 0);
    if ( base->block_count < start+len ) {
        fprintf(stderr, "[slice] can't create slice: too large for the underlying device\n");
        return NULL;
    }

    // no need to slice the whole device
    if ( start == 0 && len == base->block_count )
        return base;

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't malloc space for slice:bdev");

    struct slice_io *io;
    if ( (io = malloc(sizeof(struct slice_io))) == NULL )
        err(1, "Couldn't malloc space for slice:io");

    io->base  = base;
    io->start = start;
    io->len   = len;

    dev->m           = io;
    dev->block_size  = base->block_size;
    dev->block_count = len;

    dev->read_bytes   = slice_read_bytes;
    dev->write_bytes  = slice_write_bytes;
    dev->read_block   = slice_read_block;
    dev->write_block  = slice_write_block;
    dev->close        = slice_close;
    dev->clear_caches = slice_clear_caches;
    dev->flush        = slice_flush;

    return dev;
};

