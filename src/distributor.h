#ifndef __DISTRIBUTOR_H__
#define __DISTRIBUTOR_H__

#include <inttypes.h>
#include <stdbool.h>

struct distributor {
    int ct;
    uint8_t *d;
};

bool dis_read (struct distributor *dis, uint64_t offset, uint32_t size, void *buf);
bool dis_write(struct distributor *dis, uint64_t offset, uint32_t size, void *buf);

struct distributor * dis_create(void);

#endif

