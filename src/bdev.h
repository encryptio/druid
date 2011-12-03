#ifndef __BDEV_H__
#define __BDEV_H__

#include <inttypes.h>
#include <stdbool.h>

struct bdev {
    void *m; // extra device-specific information

    uint64_t block_size;
    uint64_t block_count;

    bool (*read_block  )(struct bdev *self, uint64_t which, uint8_t *into);
    bool (*write_block )(struct bdev *self, uint64_t which, uint8_t *from);
    void (*close       )(struct bdev *self);
    void (*reinitialize)(struct bdev *self);
};

#endif
