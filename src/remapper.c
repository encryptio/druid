#include "remapper.h"

#include <err.h>
#include <string.h>
#include <stdlib.h>

bool rm_read(struct remapper *rm, uint64_t offset, uint32_t size, void *buf) {
    if ( offset+size > rm->size )
        return false;

    memcpy(buf, rm->buf+offset, size);

    return true;
}

bool rm_write(struct remapper *rm, uint64_t offset, uint32_t size, void *buf) {
    if ( offset+size > rm->size )
        return false;

    memcpy(rm->buf+offset, buf, size);

    return true;
}

uint64_t rm_size(struct remapper *rm) {
    return rm->size;
}

struct remapper * rm_create(uint64_t size) {
    struct remapper *rm;
    if ( (rm = malloc(sizeof(struct remapper))) == NULL )
        err(1, "Couldn't allocate space for remapper");

    rm->size = size;

    if ( (rm->buf = malloc(size)) == NULL )
        err(1, "Couldn't allocate space for remapper buffer");

    return rm;
}

