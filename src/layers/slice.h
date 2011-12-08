#ifndef __LAYERS_SLICE_H__
#define __LAYERS_SLICE_H__

#include "bdev.h"

struct bdev *slice_open(struct bdev *base, uint64_t start, uint64_t len);

#endif
