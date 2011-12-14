#include "layers/xor.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "logger.h"

/*
 * There is no header for the XOR parity, all-zero devices are valid.
 *
 * Arranging the devices vertically:
 *     D1   D2   D3   D4
 *     p1_1 d2_1 d3_1 d4_1
 *     d1_2 p2_2 d3_2 d4_2
 *     d1_3 d2_3 p3_3 d4_3
 *     ...
 *
 * "slices" are the rows of this matrix.
 * The device portrayed by the xor code is the concatenation, column-major,
 * of the devices in the pool, excluding parity blocks.
 *
 * As implied by the matrix, parity is shifted to a new drive for each slice,
 * starting with the first parity on the first drive.
 */

struct xor_io {
    struct bdev **devices;
    int count;

    // cached data for a single slice
    // for write performance, mainly
    // note that the parity is inserted into its location where it will be on disk
    uint8_t *slice;
    uint64_t slice_index;
    bool slice_dirty;
};

// TODO: doing all IO in slices is slow in random-read and random-write cases.
// should we load the slice lazily and hold which have been loaded by a bitfield?
// this would allow random reads and writes to run at full speed, without harming
// the sequential write case.

static bool xor_flush_slice(struct bdev *self) {
    struct xor_io *io = self->m;
    assert(io->slice_index != -1);

    // TODO: do we consider the write failed if any drives fail to write (ensuring loss of protection)
    // or if any two do (ensuring immediate data loss)?

    int failed = 0;
    for (int i = 0; i < io->count; i++)
        if ( !io->devices[i]->write_block(io->devices[i], io->slice_index, io->slice + i*self->block_size) ) {
            failed++;
            if ( failed > 1 ) {
                logger(LOG_ERR, "xor", "Couldn't write multiple blocks to underlying devices, failing stripe write");
                return false;
            } else {
                logger(LOG_ERR, "xor", "Couldn't write block to underlying device");
            }
        }

    io->slice_dirty = false;

    return true;
}

static void xor_rebuild_slice_part(struct bdev *self, uint64_t slice, int failed) {
    struct xor_io *io = self->m;

    // TODO: optimize with word operations, consider optional SSE
    for (int i = 0; i < self->block_size; i++) {
        uint8_t b = 0;
        for (int j = 0; j < io->count; j++)
            if ( j != failed )
                b ^= io->slice[j*self->block_size + i];

        io->slice[failed*self->block_size + i] = b;
    }
}

static bool xor_switch_slice(struct bdev *self, uint64_t slice) {
    struct xor_io *io = self->m;

    if ( io->slice_index == slice )
        return true;

    if ( io->slice_dirty )
        if ( !xor_flush_slice(self) )
            return false;

    io->slice_index = -1;

    int which_failed = -1;
    for (int i = 0; i < io->count; i++)
        if ( !io->devices[i]->read_block(io->devices[i], slice, io->slice + i*self->block_size) ) {
            if ( which_failed != -1 )
                return false;
            which_failed = i;
        }

    if ( which_failed != -1 ) {
        xor_rebuild_slice_part(self, slice, which_failed);
        if ( !io->devices[which_failed]->write_block(io->devices[which_failed], slice, io->slice + which_failed*self->block_size) )
            logger(LOG_ERR, "xor", "Repaired slice, but couldn't write repaired data to disk. Ignoring.");
    }

    io->slice_index = slice;

    return true;
}

static bool xor_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    struct xor_io *io = self->m;
    assert(which < self->block_count);

    uint64_t slice = which / (io->count-1);

    uint64_t data_at = which % (io->count-1);
    uint64_t parity_at = slice % io->count;
    if ( parity_at <= data_at )
        data_at++;
    assert(parity_at != data_at);
    assert(data_at < io->count);
    assert(parity_at < io->count);

    // TODO: reuse *into as a temporary, skipping loading the whole slice if possible

    if ( !xor_switch_slice(self, slice) )
        return false;

    memcpy(into, io->slice + data_at*self->block_size, self->block_size);

    return true;
}

static bool xor_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    struct xor_io *io = self->m;
    assert(which < self->block_count);

    uint64_t slice = which / (io->count-1);

    uint64_t data_at = which % (io->count-1);
    uint64_t parity_at = slice % io->count;
    if ( parity_at <= data_at )
        data_at++;
    assert(parity_at != data_at);
    assert(data_at < io->count);
    assert(parity_at < io->count);

    // TODO: random writes are slow

    if ( !xor_switch_slice(self, slice) )
        return false;

    // switch the parity from the old block to the new one
    // TODO: optimize, SSE, etc
    for (int i = 0; i < self->block_size; i++)
        io->slice[parity_at*self->block_size + i]
            ^= from[i] ^ io->slice[data_at*self->block_size + i];

    memcpy(io->slice + data_at*self->block_size, from, self->block_size);
    io->slice_dirty = true;

    return true;
}

static void xor_close(struct bdev *self) {
    struct xor_io *io = self->m;
    free(io->devices);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void xor_clear_caches(struct bdev *self) {
    struct xor_io *io = self->m;

    xor_flush_slice(self);
    io->slice_index = -1;

    for (int i = 0; i < io->count; i++)
        io->devices[i]->clear_caches(io->devices[i]);
}

static void xor_flush(struct bdev *self) {
    struct xor_io *io = self->m;

    xor_flush_slice(self);

    for (int i = 0; i < io->count; i++)
        io->devices[i]->flush(io->devices[i]);
}

static void xor_sync(struct bdev *self) {
    xor_flush(self);

    struct xor_io *io = self->m;
    for (int i = 0; i < io->count; i++)
        io->devices[i]->sync(io->devices[i]);
}

struct bdev *xor_open(struct bdev **devices, int count) {
    if ( count < 3 ) {
        logger(LOG_ERR, "xor", "Can't create an xor device atop less than three devices (have %d devices)", count);
        return NULL;
    }

    // make sure all the block sizes are identical
    uint64_t block_size = devices[0]->block_size;
    for (int i = 1; i < count; i++) {
        assert(devices[i]);

        if ( devices[i]->block_size != block_size ) {
            logger(LOG_ERR, "xor", "Can't xor devices with"
                    "different block sizes (%llu and %llu)",
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
        logger(LOG_WARN, "xor", "Some disks in array are smaller than others. "
                "will truncate long drives to %llu blocks. "
                "(longest = %llu blocks)",
                min_drive_size, max_drive_size);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't malloc space for xor:bdev");

    struct xor_io *io;
    if ( (io = malloc(sizeof(struct xor_io))) == NULL )
        err(1, "Couldn't malloc space for xor:io");

    if ( (io->devices = malloc(sizeof(struct bdev *)*count)) == NULL )
        err(1, "Couldn't malloc space for xor:io:devices");

    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't malloc space for xor:generic_block_buffer");

    io->count = count;
    memcpy(io->devices, devices, sizeof(struct bdev *)*count);

    if ( (io->slice = malloc(block_size*count)) == NULL )
        err(1, "Couldn't malloc space for xor:io:slice");
    io->slice_index = -1;
    io->slice_dirty = false;

    dev->m           = io;
    dev->block_size  = block_size;
    dev->block_count = min_drive_size * (count-1);

    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;

    dev->read_block   = xor_read_block;
    dev->write_block  = xor_write_block;
    dev->close        = xor_close;
    dev->clear_caches = xor_clear_caches;
    dev->flush        = xor_flush;
    dev->sync         = xor_sync;

    return dev;
}

