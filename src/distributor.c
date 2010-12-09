#include "distributor.h"

#include <string.h>
#include <stdlib.h>
#include <err.h>

#define DIST_BLOCK_SIZE (64*1024*1024)

bool dis_read (struct distributor *dis, uint64_t offset, uint32_t size, void *buf) {
    return fio_read(dis->fioh, offset, size, buf);
}

bool dis_write(struct distributor *dis, uint64_t offset, uint32_t size, void *buf) {
    return fio_write(dis->fioh, offset, size, buf);
}

bool dis_commit(struct distributor *dis) {
    return fio_commit(dis->fioh);
}

struct distributor * dis_create(struct fioh *fioh) {
    struct distributor *dis;
    if ( (dis = malloc(sizeof(struct distributor))) == NULL )
        err(1, "Couldn't allocate space for distributor");

    dis->fioh = fioh;

    return dis;
}

