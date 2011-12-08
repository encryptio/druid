#ifndef __LAYERS_CONCAT_H__
#define __LAYERS_CONCAT_H__

#include "bdev.h"

struct bdev *concat_open(struct bdev **devices, int count);

#endif
