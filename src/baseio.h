#ifndef __BASEIO_H__
#define __BASEIO_H__

#include "bdev.h"
#include <unistd.h> // for size_t
#include <fcntl.h> // for off_t

struct bdev *bio_create_malloc(uint64_t block_size, size_t blocks);
struct bdev *bio_create_mmap(uint64_t block_size, int fd, size_t blocks, off_t offset);

#endif