#ifndef __DISTRIBUTOR_H__
#define __DISTRIBUTOR_H__

#include "fileio.h"

#include <inttypes.h>
#include <stdbool.h>

struct distributor {
    struct fioh *fioh;
};

bool dis_read (struct distributor *dis, uint64_t offset, uint32_t size, void *buf);
bool dis_write(struct distributor *dis, uint64_t offset, uint32_t size, void *buf);
bool dis_commit(struct distributor *dis);

struct distributor * dis_create(struct fioh *fioh);

#endif

