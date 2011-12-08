#ifndef __LAYERS_ENCRYPT_H__
#define __LAYERS_ENCRYPT_H__

#include "bdev.h"

bool encrypt_create(struct bdev *dev, const uint8_t *key, int keylen);
struct bdev *encrypt_open(struct bdev *base, const uint8_t *key, int keylen);

#endif

