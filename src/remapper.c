#include "remapper.h"

#include <err.h>
#include <string.h>
#include <stdlib.h>

bool rm_read(struct remapper *rm, uint64_t offset, uint32_t size, void *buf) {
    if ( offset+size > rm->block_size * rm->block_count )
        return false;

    uint64_t block_idx = offset / rm->block_size;

    // read the first part of the first block, if neccessary
    uint32_t our_offset = offset - block_idx * rm->block_size;
    if ( our_offset > 0 ) {
        int32_t our_read = rm->block_size - our_offset;
        if ( our_offset + size > rm->block_size ) {
            // read stretches over the first partial block
            if ( rm->block_map[block_idx] ) {
                memcpy(buf, rm->block_map[block_idx] + our_offset, our_read);
            } else {
                memset(buf, 0, our_read);
            }
            size -= our_read;
            buf  += our_read;
            block_idx++;

        } else {

            // read is entirely enclosed inside this block
            if ( rm->block_map[block_idx] ) {
                memcpy(buf, rm->block_map[block_idx] + our_offset, size);
            } else {
                memset(buf, 0, size);
            }

            return true;
        }
    }

    // now our read head is aligned with the block boundaries.
    
    // read entire blocks until we need a partial
    while ( size > rm->block_size ) {
        if ( rm->block_map[block_idx] ) {
            memcpy(buf, rm->block_map[block_idx], rm->block_size);
        } else {
            memset(buf, 0, rm->block_size);
        }

        size -= rm->block_size;
        buf  += rm->block_size;
        block_idx++;
    }

    // now read the last partial, if we need to
    if ( size ) {
        if ( rm->block_map[block_idx] ) {
            memcpy(buf, rm->block_map[block_idx], size);
        } else {
            memset(buf, 0, size);
        }
    }

    return true;
}

bool rm_write(struct remapper *rm, uint64_t offset, uint32_t size, void *buf) {
    if ( offset+size > rm->block_size * rm->block_count )
        return false;

    uint64_t block_idx = offset / rm->block_size;

    // write the first part of the first block, if neccessary
    uint32_t our_offset = offset - block_idx * rm->block_size;
    if ( our_offset > 0 ) {
        int32_t our_write = rm->block_size - our_offset;
        if ( our_offset + size > rm->block_size ) {
            // write stretches over the first partial block
            if ( !rm->block_map[block_idx] )
                if ( (rm->block_map[block_idx] = malloc(rm->block_size)) == NULL )
                    err(1, "Couldn't allocate space for block %llu", block_idx);
            memcpy(rm->block_map[block_idx] + our_offset, buf, our_write);

            size -= our_write;
            buf  += our_write;
            block_idx++;

        } else {

            // write is entirely enclosed inside this block
            if ( !rm->block_map[block_idx] )
                if ( (rm->block_map[block_idx] = malloc(rm->block_size)) == NULL )
                    err(1, "Couldn't allocate space for block %llu", block_idx);
            memcpy(rm->block_map[block_idx] + our_offset, buf, size);

            return true;
        }
    }

    // now our read head is aligned with the block boundaries.
    
    // read entire blocks until we need a partial
    while ( size > rm->block_size ) {
        if ( !rm->block_map[block_idx] )
            if ( (rm->block_map[block_idx] = malloc(rm->block_size)) == NULL )
                err(1, "Couldn't allocate space for block %llu", block_idx);
        memcpy(rm->block_map[block_idx], buf, rm->block_size);

        size -= rm->block_size;
        buf  += rm->block_size;
        block_idx++;
    }

    // now read the last partial, if we need to
    if ( size ) {
        if ( !rm->block_map[block_idx] )
            if ( (rm->block_map[block_idx] = malloc(rm->block_size)) == NULL )
                err(1, "Couldn't allocate space for block %llu", block_idx);
        memcpy(rm->block_map[block_idx], buf, size);
    }

    return true;
}

uint64_t rm_size(struct remapper *rm) {
    return rm->block_size * rm->block_count;
}

struct remapper * rm_create(uint32_t block_size, uint64_t block_count) {
    struct remapper *rm;
    if ( (rm = malloc(sizeof(struct remapper))) == NULL )
        err(1, "Couldn't allocate space for remapper");

    rm->block_size  = block_size;
    rm->block_count = block_count;

    if ( (rm->block_map = malloc(sizeof(void*) * block_count)) == NULL )
        err(1, "Couldn't allocate space for remapper map");

    for (int i = 0; i < block_count; i++)
        rm->block_map[i] = NULL;

    return rm;
}

