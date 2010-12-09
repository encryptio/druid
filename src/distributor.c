#include "distributor.h"

#include <string.h>
#include <stdlib.h>
#include <err.h>

#define DIST_BLOCK_SIZE (64*1024*1024)

bool dis_read (struct distributor *dis, uint64_t offset, uint32_t size, void *buf) {
    uint64_t device_size = DIST_BLOCK_SIZE * ((uint64_t) dis->ct);
    if ( offset >= device_size ) {
        memset(buf, 0, size);
        return true;
    }

    if ( offset+size > device_size ) {
        uint64_t overflow = device_size - offset+size;
        uint64_t overoff = size-overflow;
        memset(buf+overoff, 0, overflow);
        size -= overflow;
    }

    if ( size > 0 )
        memcpy(buf, dis->d+offset, size);

    return true;
}

bool dis_write(struct distributor *dis, uint64_t offset, uint32_t size, void *buf) {
    uint64_t blocks_needed = (offset+size+1)/DIST_BLOCK_SIZE+1;
    if ( dis->ct < blocks_needed ) {
        dis->ct = blocks_needed;
        if ( (dis->d = realloc(dis->d, blocks_needed*DIST_BLOCK_SIZE)) == NULL )
            err(1, "Couldn't realloc space for distributor block data");
    }

    memcpy(dis->d+offset, buf, size);

    return true;
}

struct distributor * dis_create(void) {
    struct distributor *dis;
    if ( (dis = malloc(sizeof(struct distributor))) == NULL )
        err(1, "Couldn't allocate space for distributor");

    dis->ct = 0;
    dis->d = NULL;

    return dis;
}

