#ifndef __LAYERS_LAZYZERO_H__
#define __LAYERS_LAZYZERO_H__

#include "bdev.h"

bool lazyzero_create(struct bdev *base);
struct bdev *lazyzero_open(struct bdev *base);

#endif
