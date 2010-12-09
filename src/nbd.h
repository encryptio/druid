#ifndef __NBD_H__
#define __NBD_H__

#include <inttypes.h>

#include "remapper.h"

struct nbd_server {
    struct remapper *rm;
    uint16_t port;
    uint32_t partition;
};

struct nbd_server * nbd_create(uint16_t port, struct remapper *rm, uint32_t partition);
void nbd_listenloop(struct nbd_server *nbd);

#endif

