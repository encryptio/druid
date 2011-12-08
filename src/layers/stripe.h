#ifndef __LAYERS_STRIPE_H__
#define __LAYERS_STRIPE_H__

#include "bdev.h"

/*
 * stripe, similar to RAID 0.
 */

struct bdev *stripe_open(struct bdev **devices, int count);

#endif
