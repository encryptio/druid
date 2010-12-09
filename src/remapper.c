#include "remapper.h"

#include "distributor.h"
#include "endian.h"

#include <err.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// invariant: calls to this function fit entirely within one block
static bool rm_read_mapped_chunk(struct remapper *rm, uint64_t block_idx, uint64_t offset, uint32_t size, void *dst) {
    uint8_t buffer[8];
    if ( !dis_read(rm->dis, rm->block_size * rm->jump_table_offset + 8 + block_idx*8, 8, buffer) )
        return false;

    uint64_t this_offset = unpack_be64(buffer);

    if ( this_offset == 0 ) {
        memset(dst, 0, size);
        return true;
    } else {
        return dis_read(rm->dis, rm->block_size * this_offset + offset, size, dst);
    }
}

bool rm_read(struct remapper *rm, uint32_t partition, uint64_t offset, uint32_t size, void *buf) {
    if ( partition > rm->partition_ct )
        return false;

    uint64_t block_idx  = offset / rm->block_size;
    uint64_t skip_early = offset - block_idx * rm->block_size;

    int extent_idx = 0;

    if ( (offset + size + rm->block_size - 1)/rm->block_size > rm->partition_info[partition].size / rm->block_size ) {
        fprintf(stderr, "[remapper] DANGER: read failed because it was out of the partition's size\n");
        return false;
    }

    // block_idx is the offset into the current extent

    while ( size > 0 ) {
        while ( block_idx >= rm->partition_info[partition].extents[extent_idx].length ) {
            block_idx -= rm->partition_info[partition].extents[extent_idx].length;
            extent_idx++;
            assert(extent_idx < rm->partition_info[partition].extent_ct);
        }

        // real_block_idx is the offset into the mapper region
        uint64_t real_block_idx = block_idx + rm->partition_info[partition].extents[extent_idx].start_block;

        if ( skip_early ) {
            // partial first read
            if ( skip_early + size <= rm->block_size ) {
                // entire read fits in one block and is not aligned to the start of it
                return rm_read_mapped_chunk(rm, real_block_idx, skip_early, size, buf);
            } else {
                uint32_t read_size = rm->block_size - skip_early;
                if ( !rm_read_mapped_chunk(rm, real_block_idx, skip_early, read_size, buf) )
                    return false;
                buf += read_size;
                size -= read_size;
                skip_early = 0;
            }
        } else {
            // read is aligned with the start of the block
            if ( size <= rm->block_size ) {
                // rest of the read fits entirely in this block
                return rm_read_mapped_chunk(rm, real_block_idx, 0, size, buf);
            } else {
                // read a complete interior block and move on
                if ( !rm_read_mapped_chunk(rm, real_block_idx, 0, rm->block_size, buf) )
                    return false;
                buf += rm->block_size;
                size -= rm->block_size;
            }
        }

        block_idx++;
    }

    err(1, "NOT REACHED");
}

// invariant: calls to this function fit entirely within one block
static bool rm_write_mapped_chunk(struct remapper *rm, uint64_t block_idx, uint64_t offset, uint32_t size, void *dst) {
    uint8_t buffer[8];
    if ( !dis_read(rm->dis, rm->block_size * rm->jump_table_offset + 8 + block_idx*8, 8, buffer) )
        return false;

    uint64_t this_offset = unpack_be64(buffer);

    if ( this_offset == 0 ) {
        // block not allocated yet
        this_offset = rm->next_free_block_index++;

        // update header information
        pack_be64(rm->next_free_block_index, buffer);
        if ( !dis_write(rm->dis, 36, 8, buffer) )
            return false;

        // update jump table entry
        pack_be64(this_offset, buffer);
        if ( !dis_write(rm->dis, rm->block_size * rm->jump_table_offset + 8 + block_idx*8, 8, buffer) )
            return false;
    }

    return dis_write(rm->dis, rm->block_size * this_offset + offset, size, dst);
}

bool rm_write(struct remapper *rm, uint32_t partition, uint64_t offset, uint32_t size, void *buf) {
    if ( partition > rm->partition_ct )
        return false;

    uint64_t block_idx  = offset / rm->block_size;
    uint64_t skip_early = offset - block_idx * rm->block_size;

    int extent_idx = 0;

    if ( (offset + size + rm->block_size - 1)/rm->block_size > rm->partition_info[partition].size / rm->block_size ) {
        fprintf(stderr, "[remapper] DANGER: write failed because it was out of the partition's size\n");
        return false;
    }

    // block_idx is the offset into the current extent

    while ( size > 0 ) {
        while ( block_idx >= rm->partition_info[partition].extents[extent_idx].length ) {
            block_idx -= rm->partition_info[partition].extents[extent_idx].length;
            extent_idx++;
            assert(extent_idx < rm->partition_info[partition].extent_ct);
        }

        // real_block_idx is the offset into the mapper region
        uint64_t real_block_idx = block_idx + rm->partition_info[partition].extents[extent_idx].start_block;

        if ( skip_early ) {
            // partial first write
            if ( skip_early + size <= rm->block_size ) {
                // entire write fits in one block and is not aligned to the start of it
                return rm_write_mapped_chunk(rm, real_block_idx, skip_early, size, buf);
            } else {
                uint32_t write_size = rm->block_size - skip_early;
                if ( !rm_write_mapped_chunk(rm, real_block_idx, skip_early, write_size, buf) )
                    return false;
                buf += write_size;
                size -= write_size;
                skip_early = 0;
            }
        } else {
            // read is aligned with the start of the block
            if ( size <= rm->block_size ) {
                // rest of the read fits entirely in this block
                return rm_write_mapped_chunk(rm, real_block_idx, 0, size, buf);
            } else {
                // read a complete interior block and move on
                if ( !rm_write_mapped_chunk(rm, real_block_idx, 0, rm->block_size, buf) )
                    return false;
                buf += rm->block_size;
                size -= rm->block_size;
            }
        }

        block_idx++;
    }

    err(1, "NOT REACHED");
}

uint64_t rm_size(struct remapper *rm, uint32_t partition) {
    if ( partition > rm->partition_ct )
        return 0;

    return rm->partition_info[partition].size;
}

bool rm_create(struct distributor *dis, uint32_t block_size, uint64_t block_count) {
    uint8_t buffer[128];

    fprintf(stderr, "[remapper] creating new remapper set, block size %d, with %d blocks in the initial partition\n", block_size, block_count);

    uint64_t block_table_end = ((block_size*2 + 8*(1+block_count)) + block_size - 1) / block_size;

    memcpy(buffer, "REMAPPER", 8);
    pack_be32(block_size, buffer+8); // block size
    pack_be64(block_count, buffer+12); // block count
    pack_be64(1, buffer+20); // partition information location
    pack_be64(2, buffer+28); // jump table location
    pack_be64(block_table_end, buffer+36); // next free block index

    if ( !dis_write(dis, 0, 44, buffer) )
        return false;

    fprintf(stderr, "[remapper] wrote header\n");

    memcpy(buffer, "PARTTBLS", 8);
    pack_be32(1, buffer+8); // number of partitions

    pack_be16(8, buffer+12); // name length
    memcpy(buffer+14, "unnamedp", 8); // name
    pack_be32(0, buffer+22); // flags
    pack_be64(block_size * block_count, buffer+26); // partition size in bytes
    pack_be64(0, buffer+34); // created timestamp
    pack_be64(0, buffer+42); // reshaped timestamp

    // uuid
    for (int i = 0; i < 16; i++)
        buffer[50+i] = random() & 0xFF;
    buffer[50+6] = (buffer[50+6] & 0x0F) | 0x40;
    buffer[50+8] = (buffer[50+8] & 0xFB);

    pack_be16(1, buffer+66); // number of extents
    pack_be64(0, buffer+68); // start block
    pack_be64(block_count-1, buffer+76); // end block
    pack_be64(block_count, buffer+84); // length in blocks

    if ( !dis_write(dis, block_size, 92, buffer) )
        return false;

    fprintf(stderr, "[remapper] wrote partition table\n");

    memcpy(buffer, "JUMPTBLS", 8);

    if ( !dis_write(dis, block_size*2, 8, buffer) )
        return false;

    memset(buffer, 0, 128);

    uint64_t at = block_size*2+8;
    uint64_t todo = block_count*8;
    while ( todo > 128 ) {
        if ( !dis_write(dis, at, 128, buffer) )
            return false;
        at += 128;
        todo -= 128;
    }

    if ( todo )
        if ( !dis_write(dis, at, todo, buffer) )
            return false;

    fprintf(stderr, "[remapper] wrote jump table\n");

    return true;
}

struct remapper * rm_open(struct distributor *dis) {
    struct remapper *rm;
    if ( (rm = malloc(sizeof(struct remapper))) == NULL )
        err(1, "Couldn't allocate space for remapper");

    rm->dis = dis;
    rm->partition_info = NULL;

    uint8_t buffer[256];

    if ( !dis_read(dis, 0, 8, buffer) )
        goto FAIL;

    if ( memcmp(buffer, "REMAPPER", 8) != 0 ) {
        fprintf(stderr, "[remapper] ERROR: bad magic in header\n");
        if ( memcmp(buffer, "\0\0\0\0\0\0\0\0", 8) == 0 )
            fprintf(stderr, "[remapper] have you created the remapper data yet?\n");

        goto FAIL;
    }

    if ( !dis_read(dis, 8, 36, buffer) )
        goto FAIL;

    rm->block_size  = unpack_be32(buffer);
    rm->block_count = unpack_be64(buffer+4);
    rm->partition_info_offset = unpack_be64(buffer+12);
    rm->jump_table_offset = unpack_be64(buffer+20);
    rm->next_free_block_index = unpack_be64(buffer+28);

    if ( !dis_read(dis, rm->block_size*rm->partition_info_offset, 12, buffer) )
        goto FAIL;

    if ( memcmp(buffer, "PARTTBLS", 8) != 0 ) {
        fprintf(stderr, "[remapper] ERROR: bad magic in partition info table\n");
        goto FAIL;
    }

    rm->partition_ct = unpack_be32(buffer+8);

    if ( (rm->partition_info = malloc(sizeof(struct partition_info)*rm->partition_ct)) == NULL )
        err(1, "Couldn't allocate space for partition info list");

    for (int i = 0; i < rm->partition_ct; i++) {
        rm->partition_info[i].name = NULL;
        rm->partition_info[i].extents = NULL;
    }

    uint64_t at = rm->block_size * rm->partition_info_offset + 12;
    for (int i = 0; i < rm->partition_ct; i++) {
        if ( !dis_read(dis, at, 2, buffer) )
            goto FAIL;
        at += 2;

        uint16_t name_length = unpack_be16(buffer);

        if ( name_length > 255 ) {
            fprintf(stderr, "[remapper] ERROR: name too long\n");
            goto FAIL;
        }

        if ( !dis_read(dis, at, name_length, buffer) )
            goto FAIL;
        at += name_length;

        buffer[name_length] = '\0';

        if ( (rm->partition_info[i].name = strdup((char*) buffer)) == NULL )
            err(1, "Couldn't allocate memory for name string");

        if ( !dis_read(dis, at, 46, buffer) )
            goto FAIL;
        at += 46;

        rm->partition_info[i].flags    = unpack_be32(buffer);
        rm->partition_info[i].size     = unpack_be64(buffer+4);
        rm->partition_info[i].created  = unpack_be64(buffer+12);
        rm->partition_info[i].reshaped = unpack_be64(buffer+20);
        for (int j = 0; j < 16; j++)
            rm->partition_info[i].uuid[j] = buffer[28+j];
        rm->partition_info[i].extent_ct = unpack_be16(buffer+44);

        if ( rm->partition_info[i].flags & PARTITION_FLAGS_RESERVED ) {
            fprintf(stderr, "[remapper] ERROR: partition %d has some reserved flags set\n", i);
            goto FAIL;
        }

        if ( (rm->partition_info[i].size % rm->block_size) != 0 ) {
            fprintf(stderr, "[remapper] ERROR: partition %d has a size that is not a multiple of the block size\n", i);
            goto FAIL;
        }

        if ( (rm->partition_info[i].extents = malloc(sizeof(struct partition_extent) * rm->partition_info[i].extent_ct)) == NULL )
            err(1, "Couldn't allocate space for extent list");

        uint64_t total_blocks_in_extents = 0;

        for (int j = 0; j < rm->partition_info[i].extent_ct; j++) {
            if ( !dis_read(dis, at, 24, buffer) )
                goto FAIL;
            at += 24;

            uint64_t st  = rm->partition_info[i].extents[j].start_block = unpack_be64(buffer);
            uint64_t en  = rm->partition_info[i].extents[j].end_block   = unpack_be64(buffer+8);
            uint64_t len = rm->partition_info[i].extents[j].length      = unpack_be64(buffer+16);

            if ( st > en ) {
                fprintf(stderr, "[remapper] ERROR: partition %d extent %d ends before it starts\n", i, j);
                goto FAIL;
            }

            if ( en >= rm->block_count ) {
                fprintf(stderr, "[remapper] ERROR: partition %d extent %d reaches outside the remapper range\n", i, j);
                goto FAIL;
            }

            if ( len != en-st+1 ) {
                fprintf(stderr, "[remapper] ERROR: partition %d extent %d has incorrect length\n", i, j);
                goto FAIL;
            }

            total_blocks_in_extents += len;
        }

        if ( total_blocks_in_extents != rm->partition_info[i].size / rm->block_size ) {
            fprintf(stderr, "[remapper] ERROR: partition %d's size is inconsistent with its extent list\n", i);
            goto FAIL;
        }
    }

    if ( !dis_read(dis, rm->block_size * rm->jump_table_offset, 8, buffer) )
        goto FAIL;

    if ( memcmp(buffer, "JUMPTBLS", 8) != 0 ) {
        fprintf(stderr, "[remapper] ERROR: bad magic in jump table\n");
        goto FAIL;
    }

    return rm;

FAIL:
    if ( rm->partition_info ) {
        for (int i = 0; i < rm->partition_ct; i++) {
            free(rm->partition_info[i].name);
            free(rm->partition_info[i].extents);
        }
        free(rm->partition_info);
    }

    free(rm);
    return NULL;
}

