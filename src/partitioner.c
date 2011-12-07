#include "partitioner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "bdev.h"
#include "endian-fns.h"

/* 
 *  disk format:
 *      header block
 *      usage bitmap
 *      mapping blocks
 *      data blocks
 *
 *  the partitions are laid out contiguously on the mapping blocks,
 *  in the order they are defined in the header block.
 *
 *  the maximum number of partitions is 61.
 *
 *  the last mapping block is zero-padded.
 *
 *  header block (512B):
 *      magic number "PART0000"
 *      uint64_t number of blocks in device
 *      uint64_t block size
 *      repeated:
 *          uint64_t number of blocks in this partition (0 if no partition defined)
 *
 *  usage bitmap:
 *      packed bits, each one representing a physical block.
 *      1 if used, 0 if free.
 *
 *  mapping blocks:
 *      repeated:
 *          uint64_t physical block location, or 0 if unmapped
 */


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

    bool old = bit_get(io->bitmap_block, interior_index);

    if ( used ) {
        bit_set(io->bitmap_block, interior_index);
        if ( !old ) io->blocks_used++;
    } else {
        bit_clear(io->bitmap_block, interior_index);
        if ( old ) io->blocks_used--;
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
// public class methods

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
    //pack_be64(10000ULL, header+PARTITION_SIZE_OFFSET); // one partition, 10000 blocks

    bool ret = true;
    if ( !dev->write_block(dev, 0, header) ) {
        ret = false;
        goto END;
    }

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

bool partitioner_set_part_size(struct bdev *dev, int partition, uint64_t new_size) {
    if ( dev->block_size < 512 ) {
        fprintf(stderr, "[partitioner] can't edit a device with a block size less than 512 bytes");
        return false;
    }

    if ( partition < 0 || partition > MAX_PARTITION_NUMBER ) {
        fprintf(stderr, "[partitioner] bad partition number: %d\n", partition);
        return false;
    }

    struct bdev *partitioner = partitioner_open(dev, -1);
    if ( !partitioner ) return false;

    struct part_io *io = partitioner->m;
    assert(io != NULL);

    uint8_t *blockbuf, *blockbuf2;
    if ( (blockbuf = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for blockbuf");
    if ( (blockbuf2 = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for blockbuf2");

    uint64_t old_size = partitioner_get_part_size(io, partition);

    if ( new_size == old_size )
        return true;

    bool enlarging = new_size > old_size;

    if ( enlarging ) {
        // make space for new map area (overestimate by at most 1 block)
        uint64_t blocks_to_pad = (new_size-old_size + io->maps_blocks_per_map-1) / io->maps_blocks_per_map;

        // scan the map area for items that may be removed
        uint64_t bad_area_start = DATA_START(io);
        uint64_t bad_area_end   = bad_area_start+blocks_to_pad;

        fprintf(stderr, "[partitioner] scanning for blocks needing remapping\n");

        for (uint64_t i = 0; i < io->mapped_total_size; i++) {
            uint64_t maploc = partitioner_block_maploc(io, i);
            if ( maploc == 1 ) goto OKERR;
            
            if ( maploc >= bad_area_start && maploc < bad_area_end ) {
                // needs to be remapped elsewhere

                uint64_t newloc = partitioner_scan_free_block(io);
                if ( maploc == 0 ) {
                    fprintf(stderr, "[partitioner] remap failed, out of space!\n");
                    goto ERR;
                }

                fprintf(stderr, "[partitioner] remapping %llu -> %llu\n",
                        (unsigned long long) maploc,
                        (unsigned long long) newloc);

                // copy the block data
                if ( !dev->read_block( dev, maploc, blockbuf) ) goto OKERR;
                if ( !dev->write_block(dev, newloc, blockbuf) ) goto OKERR;

                // mark as used
                if ( !partitioner_mark_block_as(io, newloc, true ) ) goto OKERR;

                // write new location
                if ( !partitioner_block_set_maploc(io, i, newloc) ) goto OKERR;

                // mark old as free
                if ( !partitioner_mark_block_as(io, maploc, false) ) goto OKERR;
            }
        }

        fprintf(stderr, "[partitioner] shifting mapping blocks\n");

        // shift the mapping blocks over
        uint64_t map_shift = new_size - old_size;

        uint64_t start_shift_at = 0;
        for (int i = 0; i < partition; i++)
            start_shift_at += partitioner_get_part_size(io, i);

        start_shift_at += old_size;

        uint64_t end_shift_at = io->mapped_total_size;
        fprintf(stderr, "shift_at=%llu..%llu\n", start_shift_at, end_shift_at);

        // TODO: optimize, this is really slow
        if ( end_shift_at > 0 ) {
            for (uint64_t i = end_shift_at-1; i >= start_shift_at; i--) {
                //assert(i != (uint64_t)(-1));
                //fprintf(stderr, "shifting block %llu\n", i);
                uint64_t blk = partitioner_block_maploc(io, i);
                if ( blk == 1 ) goto ERR;
                partitioner_block_set_maploc(io, i+map_shift, blk);
                if ( i == start_shift_at ) break; // wtf overflow
            }
        }

        fprintf(stderr, "[partitioner] clearing new mapping\n");

        // clear the mappings that have been opened up for use by the partition
        for (uint64_t i = start_shift_at; i < start_shift_at+map_shift; i++)
            partitioner_block_set_maploc(io, i, 0);

        fprintf(stderr, "[partitioner] marking as used\n");

        // mark the map as used
        uint64_t map_mark_start = MAPS_START(io) + end_shift_at/io->maps_blocks_per_map;
        uint64_t map_mark_end   = MAPS_START(io) + (io->mapped_total_size - old_size + new_size + io->maps_blocks_per_map-1) / io->maps_blocks_per_map;
        for (uint64_t i = map_mark_start; i <= map_mark_end; i++)
            partitioner_mark_block_as(io, i, true);

        fprintf(stderr, "[partitioner] setting partition size in header\n");

        // finally, set the partition size
        if ( !dev->read_block(dev, 0, blockbuf) ) goto ERR;
        pack_be64(new_size, blockbuf+(partition+3)*8);
        if ( !dev->write_block(dev, 0, blockbuf) ) goto ERR;
        
    } else {
        // shrinking
        fprintf(stderr, "[partitioner] partition shrinking not implemented\n");
        goto OKERR;
    }

    free(blockbuf);
    free(blockbuf2);
    partitioner->close(partitioner);
    return true;

ERR:
    fprintf(stderr, "[partitioner] resize failed at an unlucky time. your partitions are probably corrupted.\n");
    goto BADQUIT;

OKERR:
    fprintf(stderr, "[partitioner] resize failed at a lucky time. data may be safe.\n");

BADQUIT:
    free(blockbuf);
    free(blockbuf2);
    partitioner->close(partitioner);
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// object methods

static bool part_read_block(struct bdev *dev, uint64_t which, uint8_t *into) {
    assert(which < dev->block_count);

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
    assert(which < dev->block_count);

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

static void part_flush(struct bdev *dev) {
    struct part_io *io = dev->m;
    io->base->flush(io->base);
}

static void part_clear_caches(struct bdev *dev) {
    struct part_io *io = dev->m;
    io->bitmap_block_which = 0;
    io->map_block_which    = 0;
    io->base->clear_caches(io->base);
}

////////////////////////////////////////////////////////////////////////////////
// constructor

// special case: partition=-1 means don't open a partition
struct bdev *partitioner_open(struct bdev *dev, int partition) {
    if ( dev->block_size < 512 ) {
        fprintf(stderr, "[partitioner] can't open a device with a block size less than 512 bytes");
        return NULL;
    }

    if ( partition < -1 || partition > MAX_PARTITION_NUMBER ) {
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

    if ( partition != -1 ) {
        mydev->block_count = partitioner_get_part_size(io, partition);
        if ( mydev->block_count == 0 ) {
            fprintf(stderr, "[partitioner] partition %d has not been defined in that device\n", partition);
            goto ERR;
        }
        io->mapper_partition_offset = partitioner_get_partition_offset(io, partition);
    }

    io->open_partition = partition;

    if ( partition != -1 ) {
        mydev->read_block  = part_read_block;
        mydev->write_block = part_write_block;
    } else {
        // better to cause segfaults than corrupt data
        mydev->read_block  = NULL;
        mydev->write_block = NULL;
    }
    mydev->close        = part_close;
    mydev->clear_caches = part_clear_caches;
    mydev->flush        = part_flush;

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

