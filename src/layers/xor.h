#ifndef __LAYERS_XOR_H__
#define __LAYERS_XOR_H__

#include "bdev.h"

/*
 * XOR-based parity, spread across the drives on a block-basis.
 * Similar to RAID-5.
 */

struct bdev *xor_open(struct bdev **devices, int count);

#endif
