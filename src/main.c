#include "nbd.h"
#include "baseio.h"
#include "verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    struct bdev *base = bio_create_malloc(1024, 204800); // 200 MiB
    assert(base);
    struct bdev *v = verify_create(base);
    assert(v);
    struct nbd_server *nbd = nbd_create(1234, v);
    assert(nbd);
    nbd_listenloop(nbd);
}

