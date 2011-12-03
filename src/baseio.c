#include "baseio.h"

#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <errno.h>

struct mem_io {
    uint8_t *base;
    size_t mmaplen;
};

static bool mem_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    assert(which < self->block_count);
    struct mem_io *io = self->m;
    memcpy(into, io->base + which*self->block_size, self->block_size);
    return true;
}

static bool mem_write_block(struct bdev *self, uint64_t which, uint8_t *from) {
    assert(which < self->block_count);
    struct mem_io *io = self->m;
    memcpy(io->base + which*self->block_size, from, self->block_size);
    return true;
}

static void mem_reinitialize(struct bdev *self) {
    // do nothing
    return;
}

static void mem_malloc_close(struct bdev *self) {
    struct mem_io *io = self->m;
    free(io->base);
    free(io);
    free(self);
}

static void mem_mmap_close(struct bdev *self) {
    struct mem_io *io = self->m;
    if ( !munmap(io->base, io->mmaplen) )
        err(1, "Couldn't munmap base in mem_mmap_close");
    free(io);
    free(self);
}

struct bdev *bio_create_malloc(uint64_t block_size, size_t blocks) {
    assert(block_size);
    assert(!(block_size & (block_size-1))); // is a power of two

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct mem_io))) == NULL )
        err(1, "Couldn't allocate space for bdev :: mem_io");
    struct mem_io *io = dev->m;

    dev->read_block = mem_read_block;
    dev->write_block = mem_write_block;
    dev->reinitialize = mem_reinitialize;
    dev->close = mem_malloc_close;

    dev->block_size = block_size;
    dev->block_count = blocks;

    if ( (io->base = malloc(block_size * blocks)) == NULL ) {
        fprintf(stderr, "Couldn't allocate device memory (%llu blocks of %llu bytes each", (unsigned long long) block_size, (unsigned long long) blocks);
        goto ERROR;
    }

    return dev;

ERROR:
    free(io);
    free(dev);

    return NULL;
}

struct bdev *bio_create_mmap(uint64_t block_size, int fd, size_t blocks, off_t offset) {
    assert(block_size);
    assert(!(block_size & (block_size-1))); // is a power of two

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct mem_io))) == NULL )
        err(1, "Couldn't allocate space for bdev :: mem_io");
    struct mem_io *io = dev->m;

    dev->read_block = mem_read_block;
    dev->write_block = mem_write_block;
    dev->reinitialize = mem_reinitialize;
    dev->close = mem_mmap_close;

    dev->block_size = block_size;
    dev->block_count = blocks;

    if ( (io->base = mmap(NULL, block_size * blocks, PROT_READ|PROT_WRITE, 0, fd, offset)) == NULL ) {
        fprintf(stderr, "Couldn't mmap device memory (%llu blocks of %llu bytes each: %s\n", (unsigned long long) block_size, (unsigned long long) blocks, strerror(errno));
        goto ERROR;
    }

    return dev;

ERROR:
    free(io);
    free(dev);

    return NULL;
}

