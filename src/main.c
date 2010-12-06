#include "remapper.h"
#include "nbd.h"

#include <stdio.h>

int main(void) {
    struct remapper *rm = rm_create(1024*1024*32);
    struct nbd_server *nbd = nbd_create(1234, rm);
    printf("remapper is size %d KiB\n", (int) (rm_size(rm)/1024));
    nbd_listenloop(nbd);
}

