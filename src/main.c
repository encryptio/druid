#include "distributor.h"
#include "remapper.h"
#include "nbd.h"

#include <stdio.h>

int main(void) {
    struct fioh *fioh = fio_open("mem:256m");
    struct distributor *dis = dis_create(fioh);
    rm_create(dis, 1024*32, 32*1024*10);
    struct remapper *rm = rm_open(dis);
    struct nbd_server *nbd = nbd_create(1234, rm, 0);
    printf("remapper is size %d KiB\n", (int) (rm_size(rm, 0)/1024));
    nbd_listenloop(nbd);
}

