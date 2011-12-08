#ifndef __SLICE_H__
#define __SLICE_H__

#include "bdev.h"

struct bdev *slice_open(struct bdev *base, uint64_t start, uint64_t len);

#endif
