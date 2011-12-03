#include "distributor.h"
#include "remapper.h"
#include "nbd.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    struct fioh *fioh = fio_open("file:testfile-1");
    if ( !fioh ) exit(1);
    struct distributor *dis = dis_create();
    if ( !dis ) exit(1);
    if ( !dis_intialize_fioh(fioh) ) exit(1);
    if ( !dis_add_fioh(fioh) ) exit(1);
    rm_create(dis, 1024*32, 32*1024*10);
    struct remapper *rm = rm_open(dis);
    if ( !rm ) exit(1);
    struct nbd_server *nbd = nbd_create(1234, rm, 0);
    if ( !nbd ) exit(1);
    printf("remapper is size %d KiB\n", (int) (rm_size(rm, 0)/1024));
    nbd_listenloop(nbd);
}

