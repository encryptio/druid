#ifndef __BDEV_H__
#define __BDEV_H__

#include <inttypes.h>
#include <stdbool.h>

struct bdev {
    void *m; // extra device-specific information

    uint64_t block_size;
    uint64_t block_count;

    bool (*read_bytes  )(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into);
    bool (*write_bytes )(struct bdev *self, uint64_t start, uint64_t len, uint8_t *from);
    bool (*read_block  )(struct bdev *self, uint64_t which, uint8_t *into);
    bool (*write_block )(struct bdev *self, uint64_t which, uint8_t *from);
    void (*close       )(struct bdev *self);
    void (*clear_caches)(struct bdev *self);
    void (*flush       )(struct bdev *self);

    uint8_t *generic_block_buffer;
};

// generic implementations of *_bytes that use *_block.
// if you use these functions, you must point generic_block_buffer
//      to a chunk of memory at least block_size bytes long.
bool generic_read_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into);
bool generic_write_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *from);

void generic_clear_caches(struct bdev *self);
void generic_flush(struct bdev *self);

#endif
