#ifndef __REMAPPER_H__
#define __REMAPPER_H__

#include <inttypes.h>
#include <stdbool.h>

struct remapper {
    uint32_t block_size;
    void **block_map;
    uint64_t block_count;
};

bool     rm_read (struct remapper *rm, uint64_t offset, uint32_t size, void *buf);
bool     rm_write(struct remapper *rm, uint64_t offset, uint32_t size, void *buf);
uint64_t rm_size (struct remapper *rm);

struct remapper * rm_create(uint32_t block_size, uint64_t block_count);

#endif

