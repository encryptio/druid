#ifndef __REMAPPER_H__
#define __REMAPPER_H__

#include <inttypes.h>
#include <stdbool.h>

#define PARTITION_FLAGS_BLOWFISH 0x00000001
#define PARTITION_FLAGS_RESERVED 0xFFFFFFFE

struct partition_extent {
    uint64_t start_block;
    uint64_t end_block;
    uint64_t length;
};

struct partition_info {
    char *name;
    uint32_t flags;
    uint64_t size; // in bytes
    uint64_t created;
    uint64_t reshaped;
    uint8_t uuid[16];
    uint16_t extent_ct;
    struct partition_extent *extents;
};

struct remapper {
    struct distributor *dis;

    // header info
    uint32_t block_size;
    uint64_t block_count;
    uint64_t partition_info_offset; // in blocks
    uint64_t jump_table_offset; // in blocks
    uint64_t next_free_block_index; // in blocks

    // partition info
    int partition_ct;
    struct partition_info *partition_info;

    // TODO: cache jump table
    // TODO: cache data
};

bool     rm_read (struct remapper *rm, uint32_t partition, uint64_t offset, uint32_t size, void *buf);
bool     rm_write(struct remapper *rm, uint32_t partition, uint64_t offset, uint32_t size, void *buf);
uint64_t rm_size (struct remapper *rm, uint32_t partition);

bool rm_create(struct distributor *dis, uint32_t block_size, uint64_t block_count);
struct remapper * rm_open(struct distributor *dis);

#endif

