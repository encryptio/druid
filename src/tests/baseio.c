#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <err.h>

#include "test.h"
#include "baseio.h"

void test_consistency_bdev_with_block(struct bdev *dev, uint8_t *basis, uint8_t *block) {
    for (int i = 0; i < dev->block_count; i++) {
        test(dev->write_block(dev, i, basis));
        test(dev->read_block( dev, i, block));

        test( memcmp(block, basis, dev->block_size) == 0 );
    }
    for (int i = 0; i < dev->block_count; i++) {
        test(dev->read_block(dev, i, block));
        test( memcmp(block, basis, dev->block_size) == 0 );
    }
}

void test_consistency_bdev(struct bdev *dev) {
    uint8_t *block, *basis;

    if ( (block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't malloc space for comparison block");
    if ( (basis = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't malloc space for basis block");

    // all zeroes
    memset(basis, 0x00, dev->block_size);
    test_consistency_bdev_with_block(dev, basis, block);

    // all ones
    memset(basis, 0xFF, dev->block_size);
    test_consistency_bdev_with_block(dev, basis, block);

    // odd bits
    memset(basis, 0xaa, dev->block_size);
    test_consistency_bdev_with_block(dev, basis, block);

    // even bits
    memset(basis, 0x55, dev->block_size);
    test_consistency_bdev_with_block(dev, basis, block);

    // some random byte patterns
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < dev->block_size; j++)
            basis[j] = random() & 0xFF;
        test_consistency_bdev_with_block(dev, basis, block);
    }
}

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);

    char suite_name[128];

    for (uint64_t bs = 1; bs <= 131072; bs *= 2)
        for (uint64_t blocks = 4; blocks <= 16; blocks *= 2) {
            snprintf(suite_name, 128, "bio ram bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            struct bdev *dev = bio_create_malloc(bs, blocks);

            test(dev != NULL);
            if ( dev ) {
                test_consistency_bdev(dev);
                dev->close(dev);
            }
        }

    test_exit();
}

