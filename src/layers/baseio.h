#ifndef __LAYERS_BASEIO_H__
#define __LAYERS_BASEIO_H__

#include "bdev.h"
#include <unistd.h> // for size_t
#include <fcntl.h> // for off_t

struct bdev *bio_create_malloc(uint64_t block_size, size_t blocks);
struct bdev *bio_create_mmap(uint64_t block_size, int fd, size_t blocks, off_t offset, bool inherit_fd);
struct bdev *bio_create_posixfd(uint64_t block_size, int fd, size_t blocks, off_t offset, bool inherit_fd);

#endif
