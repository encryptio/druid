#ifndef __PARTITIONER_H__
#define __PARTITIONER_H__

#include "bdev.h"

/*
 * partition numbers are in the range 0..62 inclusive
 */

bool partitioner_initialize(struct bdev *dev);
bool partitioner_set_part_size(struct bdev *dev, int partition, uint64_t blocks);
struct bdev *partitioner_open(struct bdev *dev, int partition);

#endif
