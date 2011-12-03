#ifndef __NBD_H__
#define __NBD_H__

#include <inttypes.h>

#include "bdev.h"

struct nbd_server {
    struct bdev *dev;
    uint16_t port;
};

struct nbd_server * nbd_create(uint16_t port, struct bdev *dev);
void nbd_listenloop(struct nbd_server *nbd);

#endif

