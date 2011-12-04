#include "partitioner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "bdev.h"

////////////////////////////////////////////////////////////////////////////////
// bit vector manipulation routines

static inline void bit_set(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    set[byte] |= bit;
}

static inline bool bit_get(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    return (set[byte] & bit) ? true : false;
}

static inline void bit_clear(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    set[byte] &= ~bit;
}

static inline uint32_t bit_count_in_u32(uint32_t v) {
    // from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    return ((v + ((v >> 4) & 0xF0F0F0F0)) * 0x10101010) >> 24;
}

////////////////////////////////////////////////////////////////////////////////
// part_io and related macros for the on-disk format

// ohohoho partition magic
#define PARTITION_MAGIC "PART0000"
#define MAX_PARTITION_NUMBER 60
#define PARTITION_SIZE_OFFSET 24

#define BITMAP_START(partio) (1ULL)
#define MAPS_START(partio) ((partio)->bitmap_len+BITMAP_START(partio))
#define DATA_START(partio) ((partio)->maps_len+MAPS_START(partio))

struct part_io {
    struct bdev *base;

    uint64_t bitmap_blocks_per_map;
    uint64_t maps_blocks_per_map;

    // all of the following in block counts
    uint64_t block_count;
    uint64_t blocks_used;
    uint64_t bitmap_len;
    uint64_t maps_len;
    uint64_t mapped_total_size;

    uint64_t free_scan_from; // block location, last seen free in the bitmap

    // TODO: cache bitmap_block writes using this memory
    uint8_t *bitmap_block;
    uint64_t bitmap_block_which; // physical location

    // TODO: cache map_block writes using this memory
    uint8_t *map_block;
    uint64_t map_block_which; // physical location

    int open_partition;
    uint64_t mapper_partition_offset;
};

////////////////////////////////////////////////////////////////////////////////
// helpers

// initialize all fields except base, bitmap_block, open_partition, mapper_partition_offset, and map_block
static bool partitioner_setup_io(struct part_io *io) {
    io->bitmap_blocks_per_map = io->base->block_size*8;
    io->maps_blocks_per_map   = io->base->block_size/8;

    // use bitmap_block as a temporary holding space for the header (clobbered later)
    if ( !io->base->read_block(io->base, 0, io->bitmap_block) )
        return false;

    if ( memcmp(io->bitmap_block, PARTITION_MAGIC, 8) != 0 ) {
        fprintf(stderr, "[partitioner] can't setup io on device:"
                "bad magic number\n");
        return false;
    }

    io->block_count = unpack_be64(io->bitmap_block+8);

    if ( io->block_count > io->base->block_count ) {
        fprintf(stderr, "[partitioner] can't setup io on device:"
                "block count on disk (%llu) is larger than physical block count (%llu)\n",
                (unsigned long long) io->block_count,
                (unsigned long long) io->base->block_count);
        return false;
    } else if ( io->block_count < io->base->block_count ) {
        fprintf(stderr, "[partitioner] warning: block count on disk (%llu) is smaller than "
                "physical block count (%llu). reshape to fix.\n",
                (unsigned long long) io->block_count,
                (unsigned long long) io->base->block_count);
    }

    uint64_t block_size = unpack_be64(io->bitmap_block+16);
    if ( block_size != io->base->block_size ) {
        fprintf(stderr, "[partitioner] can't setup io on device:"
                "block size on disk (%llu) is not equal to block size on device (%llu)\n",
                (unsigned long long) block_size,
                (unsigned long long) io->base->block_size);
        return false;
    }

    io->bitmap_len = (io->block_count + io->bitmap_blocks_per_map-1) / io->bitmap_blocks_per_map;

    io->mapped_total_size = 0;
    for (int i = 3; i < 64; i++)
        io->mapped_total_size += unpack_be64(io->bitmap_block+i*8);

    io->maps_len = (io->mapped_total_size + io->maps_blocks_per_map-1) / io->maps_blocks_per_map;

    io->free_scan_from = 1;

    // read all the bitmap blocks to get a blocks_used count
    // this clobbers the header read above
    io->bitmap_block_which = 0;
    io->blocks_used = 0;
    for (uint64_t i = BITMAP_START(io); i < MAPS_START(io); i++) {
        if ( !io->base->read_block(io->base, i, io->bitmap_block) )
            return false;
        io->bitmap_block_which = i;

        uint32_t *bitmap = (uint32_t*) io->bitmap_block;

        for (int j = 0; j < block_size/4; j++)
            io->blocks_used += bit_count_in_u32(bitmap[j]);
    }

    return true;
}

static bool partitioner_open_bitmap_block_for(struct part_io *io, uint64_t block) {
    uint64_t bitmap_block_wanted = block / io->bitmap_blocks_per_map + BITMAP_START(io);

    if ( bitmap_block_wanted != io->bitmap_block_which ) {
        io->bitmap_block_which = 0; // clear in case of failed read here

        if ( !io->base->read_block(io->base, bitmap_block_wanted, io->bitmap_block) )
            return false;

        io->bitmap_block_which = bitmap_block_wanted;
    }

    return true;
}

static uint64_t partitioner_scan_free_block(struct part_io *io) {
    uint64_t start_scan = io->free_scan_from;

    do {
        if ( !partitioner_open_bitmap_block_for(io, io->free_scan_from) )
            return 0;

        uint64_t interior_index = io->free_scan_from % io->bitmap_blocks_per_map;

        if ( !bit_get(io->bitmap_block, interior_index) )
            return io->free_scan_from;

        io->free_scan_from++;

        if ( io->free_scan_from >= io->block_count )
            io->free_scan_from = 1;

    } while ( start_scan != io->free_scan_from ); // if we loop around completely, we're out of space

    return 0; // failure
}

static bool partitioner_mark_block_as(struct part_io *io, uint64_t which, bool used) {
    if ( !partitioner_open_bitmap_block_for(io, which) )
        return false;

    uint64_t interior_index = which % io->bitmap_blocks_per_map;

    if ( used ) {
        bit_set(io->bitmap_block, interior_index);
    } else {
        bit_clear(io->bitmap_block, interior_index);
    }

    // TODO: cache
    if ( !io->base->write_block(io->base, io->bitmap_block_which, io->bitmap_block) ) {
        io->bitmap_block_which = 0;
        return false;
    } else {
        return true;
    }
}

static uint64_t partitioner_get_part_size(struct part_io *io, int partition) {
    io->bitmap_block_which = 0;
    io->base->read_block(io->base, 0, io->bitmap_block);

    return unpack_be64(io->bitmap_block + (partition+3)*8);
}

static uint64_t partitioner_get_partition_offset(struct part_io *io, int partition) {
    io->bitmap_block_which = 0;
    io->base->read_block(io->base, 0, io->bitmap_block);

    uint64_t offset = 0;
    for (int i = 0; i < partition; i++)
        offset += unpack_be64(io->bitmap_block + (partition+3)*8);

    return offset;
}

static bool partitioner_open_map_block_for(struct part_io *io, uint64_t which) {
    uint64_t map_block_wanted = MAPS_START(io) + which / io->maps_blocks_per_map;

    if ( map_block_wanted != io->map_block_which ) {
        io->map_block_which = 0;
        if ( !io->base->read_block(io->base, map_block_wanted, io->map_block) )
            return false;
        io->map_block_which = map_block_wanted;
    }

    return true;
}

static uint64_t partitioner_block_maploc(struct part_io *io, uint64_t which) {
    if ( !partitioner_open_map_block_for(io, which) )
        return 1; // failure, never exists in a map

    uint64_t interior = which % io->maps_blocks_per_map;
    return unpack_be64(io->map_block + 8*interior);
}

static bool partitioner_block_set_maploc(struct part_io *io, uint64_t which, uint64_t to) {
    if ( !partitioner_open_map_block_for(io, which) )
        return false;

    uint64_t interior = which % io->maps_blocks_per_map;
    pack_be64(to, io->map_block + 8*interior);

    if ( !io->base->write_block(io->base, io->map_block_which, io->map_block) ) {
        io->map_block_which = 0;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// public interface

bool partitioner_initialize(struct bdev *dev) {
    if ( dev->block_size < 512 ) {
        fprintf(stderr, "[partitioner] can't initialize a device with a block size less than 512 bytes");
        return false;
    }

    struct part_io io;
    io.base = dev;

    if ( (io.bitmap_block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate a block of ram for the bitmap_block");
    uint8_t *header = io.bitmap_block;

    memset(header, 0, dev->block_size);
    memcpy(header, PARTITION_MAGIC, 8);
    pack_be64(dev->block_count, header+8);
    pack_be64(dev->block_size, header+16);
    pack_be64(1024, header+PARTITION_SIZE_OFFSET); // one partition, 1024 blocks

    bool ret = dev->write_block(dev, 0, header);

    // XXX: what if the bitmap area is unreadable at the moment?
    if ( !partitioner_setup_io(&io) ) {
        ret = false;
        goto END;
    }

    // clear the bitmap
    io.bitmap_block_which = 0;
    memset(header, 0, dev->block_size);
    for (uint64_t i = BITMAP_START(&io); i < MAPS_START(&io); i++) {
        dev->write_block(dev, i, header);
    }

    // mark the non-data portions as used
    for (uint64_t i = 0; i < DATA_START(&io); i++)
        partitioner_mark_block_as(&io, i, true);

END:
    free(header);

    return ret;
}

bool partitioner_set_part_size(struct bdev *dev, int partition, uint64_t blocks) {
    if ( dev->block_size < 512 ) {
        fprintf(stderr, "[partitioner] can't edit a device with a block size less than 512 bytes");
        return false;
    }

    if ( partition < 0 || partition > MAX_PARTITION_NUMBER ) {
        fprintf(stderr, "[partitioner] bad partition number: %d\n", partition);
        return false;
    }

    fprintf(stderr, "[partitioner] partitioner_set_part_size not implemented\n");
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// object methods

static bool part_read_block(struct bdev *dev, uint64_t which, uint8_t *into) {
    struct part_io *io = dev->m;
    uint64_t maploc = partitioner_block_maploc(io, which+io->mapper_partition_offset);
    if ( maploc == 1 ) return false;

    if ( maploc == 0 ) {
        // not written to
        memset(into, 0, dev->block_size);
        return true;
    } else {
        return io->base->read_block(io->base, maploc, into);
    }
}

static bool part_write_block(struct bdev *dev, uint64_t which, uint8_t *from) {
    struct part_io *io = dev->m;
    uint64_t maploc = partitioner_block_maploc(io, which+io->mapper_partition_offset);
    if ( maploc == 1 ) return false;

    if ( maploc == 0 ) {
        // not written to, need to allocate it
        maploc = partitioner_scan_free_block(io);
        if ( maploc == 0 ) {
            fprintf(stderr, "[partitioner] write failed, out of space!\n");
            return false;
        }

        if ( !partitioner_block_set_maploc(io, which+io->mapper_partition_offset, maploc) )
            return false;

        if ( !partitioner_mark_block_as(io, maploc, true) )
            return false;
    }

    return io->base->write_block(io->base, maploc, from);
}

static void part_close(struct bdev *dev) {
    struct part_io *io = dev->m;
    free(io->map_block);
    free(io->bitmap_block);
    free(io);
    free(dev->generic_block_buffer);
    free(dev);
}

static void part_clear_caches(struct bdev *dev) {
    struct part_io *io = dev->m;
    io->bitmap_block_which = 0;
    io->map_block_which    = 0;
}

////////////////////////////////////////////////////////////////////////////////
// constructor

struct bdev *partitioner_open(struct bdev *dev, int partition) {
    if ( dev->block_size < 512 ) {
        fprintf(stderr, "[partitioner] can't open a device with a block size less than 512 bytes");
        return NULL;
    }

    if ( partition < 0 || partition > MAX_PARTITION_NUMBER ) {
        fprintf(stderr, "[partitioner] bad partition number: %d\n", partition);
        return false;
    }

    struct bdev *mydev;
    if ( (mydev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for partitioner bdev");
    if ( (mydev->m = malloc(sizeof(struct part_io))) == NULL )
        err(1, "Couldn't allocate space for partitioner part_io");
    struct part_io *io = mydev->m;

    if ( (io->bitmap_block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for partitioner part_io:bitmap_block");
    if ( (io->map_block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for partitioner part_io:map_block");
    io->bitmap_block_which = 0;
    io->map_block_which    = 0;

    io->base = dev;

    if ( !partitioner_setup_io(io) )
        goto ERR;

    mydev->block_size = dev->block_size;

    mydev->block_count = partitioner_get_part_size(io, partition);
    if ( mydev->block_count == 0 ) {
        fprintf(stderr, "[partitioner] partition %d has not been defined in that device\n", partition);
        goto ERR;
    }
    io->open_partition = partition;
    io->mapper_partition_offset = partitioner_get_partition_offset(io, partition);

    mydev->read_block   = part_read_block;
    mydev->write_block  = part_write_block;
    mydev->close        = part_close;
    mydev->clear_caches = part_clear_caches;
    mydev->flush        = NULL;

    if ( (mydev->generic_block_buffer = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for partitioner dev:generic_block_buffer");
    mydev->read_bytes = generic_read_bytes;
    mydev->write_bytes = generic_write_bytes;

    return mydev;

ERR:
    free(io->map_block);
    free(io->bitmap_block);
    free(io);
    free(mydev);

    return NULL;
}

