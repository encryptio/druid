#include "layers/baseio.h"

#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "logger.h"

////////////////////////////////////////////////////////////////////////////////
// Memory IO, both in-process with malloc and on-disk with mmap

struct mem_io {
    uint8_t *base;
    size_t mmaplen;
    bool inherit_fd;
    int fd;
};

static bool mem_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    assert(which < self->block_count);
    struct mem_io *io = self->m;
    memcpy(into, io->base + which*self->block_size, self->block_size);
    return true;
}

static bool mem_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    assert(which < self->block_count);
    struct mem_io *io = self->m;
    memcpy(io->base + which*self->block_size, from, self->block_size);
    return true;
}

static void mem_malloc_close(struct bdev *self) {
    struct mem_io *io = self->m;
    free(io->base);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void mem_mmap_close(struct bdev *self) {
    struct mem_io *io = self->m;
    if ( munmap(io->base, io->mmaplen) == -1 )
        err(1, "Couldn't munmap base in mem_mmap_close");

    if ( io->inherit_fd )
        if ( close(io->fd) )
            logger(LOG_ERR, "baseio", "Couldn't close filehandle: %s", strerror(errno));

    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void mem_mmap_clear_caches(struct bdev *self) {
    struct mem_io *io = self->m;
    if ( msync(io->base, io->mmaplen, MS_INVALIDATE) )
        err(1, "Couldn't msync base in mem_mmap_clear_caches");
}

static void mem_mmap_flush(struct bdev *self) {
    struct mem_io *io = self->m;
    if ( msync(io->base, io->mmaplen, MS_ASYNC) )
        err(1, "Couldn't msync base in mem_mmap_flush");
}

static void mem_mmap_sync(struct bdev *self) {
    struct mem_io *io = self->m;
    if ( msync(io->base, io->mmaplen, MS_SYNC) )
        err(1, "Couldn't msync base in mem_mmap_sync");
}

struct bdev *bio_create_malloc(uint64_t block_size, size_t blocks) {
    assert(block_size);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct mem_io))) == NULL )
        err(1, "Couldn't allocate space for bdev :: mem_io");
    struct mem_io *io = dev->m;

    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;
    dev->read_block   = mem_read_block;
    dev->write_block  = mem_write_block;
    dev->close        = mem_malloc_close;
    dev->clear_caches = generic_clear_caches;
    dev->flush        = generic_flush;
    dev->sync         = generic_sync;

    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't allocate space for generic block buffer");

    dev->block_size = block_size;
    dev->block_count = blocks;

    if ( (io->base = malloc(block_size * blocks)) == NULL ) {
        logger(LOG_ERR, "baseio", "Couldn't allocate device memory (%llu blocks of %llu bytes each)",
                (unsigned long long) block_size, (unsigned long long) blocks);
        goto ERROR;
    }

    io->fd = -1;
    io->inherit_fd = false;

    return dev;

ERROR:
    free(io);
    free(dev);

    return NULL;
}

struct bdev *bio_create_mmap(uint64_t block_size, int fd, size_t blocks, off_t offset, bool inherit_fd) {
    assert(block_size);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct mem_io))) == NULL )
        err(1, "Couldn't allocate space for bdev :: mem_io");
    struct mem_io *io = dev->m;

    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;
    dev->read_block   = mem_read_block;
    dev->write_block  = mem_write_block;
    dev->close        = mem_mmap_close;
    dev->clear_caches = mem_mmap_clear_caches;
    dev->flush        = mem_mmap_flush;
    dev->sync         = mem_mmap_sync;

    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't allocate space for generic block buffer");

    dev->block_size = block_size;
    dev->block_count = blocks;

    io->mmaplen = block_size * blocks;
    io->inherit_fd = inherit_fd;
    io->fd = fd;

    if ( (io->base = mmap(NULL, block_size * blocks, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset)) == MAP_FAILED ) {
        logger(LOG_ERR, "baseio", "Couldn't mmap device memory (%llu blocks of %llu bytes each: %s",
                (unsigned long long) block_size, (unsigned long long) blocks, strerror(errno));
        goto ERROR;
    }

    return dev;

ERROR:
    free(io);
    free(dev);

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// POSIX IO

struct fd_io {
    int fd;
    off_t offset;
    bool inherit_fd;
};

static bool fd_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    assert(which < self->block_count);
    struct fd_io *io = self->m;
    ssize_t ret = pread(io->fd, into, self->block_size, which*self->block_size+io->offset);
    if ( ret == -1 ) {
        logger(LOG_WARN, "baseio", "Couldn't read from file (fd=%d): %s",
                io->fd, strerror(errno));
        return false;
    }

    // automagically zero-pad
    if ( ret < self->block_size )
        memset(into+ret, 0, self->block_size-ret);

    return true;
}

static bool fd_write_block(struct bdev *self, uint64_t which, const uint8_t *from) {
    assert(which < self->block_count);
    struct fd_io *io = self->m;
    ssize_t ret = pwrite(io->fd, from, self->block_size, which*self->block_size+io->offset);
    if ( ret == -1 ) {
        logger(LOG_WARN, "baseio", "Couldn't write to file (fd=%d): %s",
                io->fd, strerror(errno));
        return false;
    }

    if ( ret != self->block_size ) {
        logger(LOG_WARN, "baseio", "Short write to file (fd=%d)",
                io->fd);
        return false;
    }

    return true;
}

static void fd_close(struct bdev *self) {
    struct fd_io *io = self->m;
    if ( io->inherit_fd )
        if ( close(io->fd) )
            logger(LOG_ERR, "baseio", "Couldn't close filehandle: %s", strerror(errno));

    free(self->m);
    free(self->generic_block_buffer);
    free(self);
}

static void fd_sync(struct bdev *self) {
    struct fd_io *io = self->m;
    if ( fsync(io->fd) < 0 )
        logger(LOG_ERR, "baseio", "Couldn't fsync (fd=%d): %s\n",
                io->fd, strerror(errno));
}

struct bdev *bio_create_posixfd(uint64_t block_size, int fd, size_t blocks, off_t offset, bool inherit_fd) {
    assert(block_size);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev");

    if ( (dev->m = malloc(sizeof(struct fd_io))) == NULL )
        err(1, "Couldn't allocate space for bdev:fd_io");
    struct fd_io *io = dev->m;

    dev->read_bytes   = generic_read_bytes;
    dev->write_bytes  = generic_write_bytes;
    dev->read_block   = fd_read_block;
    dev->write_block  = fd_write_block;
    dev->close        = fd_close;
    dev->clear_caches = generic_clear_caches;
    dev->flush        = generic_flush;
    dev->sync         = fd_sync;

    if ( (dev->generic_block_buffer = malloc(block_size)) == NULL )
        err(1, "Couldn't allocate space for generic block buffer");

    dev->block_size = block_size;
    dev->block_count = blocks;

    io->fd = fd;
    io->offset = offset;
    io->inherit_fd = inherit_fd;

    return dev;
}

