#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <err.h>
#include <assert.h>

#include "test.h"
#include "layers/baseio.h"
#include "layers/slice.h"

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

    free(block);
    free(basis);
}

void test_byte_interface(struct bdev *dev) {
    uint8_t *partial, *partial2;
    uint64_t size = dev->block_size * dev->block_count;
    if ( size > 1024*1024*8 )
        size = 1024*1024*8; // 8 MiB limit

    if ( (partial = malloc(size)) == NULL )
        err(1, "Couldn't malloc space for partial block device");
    if ( (partial2 = malloc(size)) == NULL )
        err(1, "Couldn't malloc space for partial block device");
    memset(partial, 0, size);

    test(dev->write_bytes(dev, 0, size, partial));

    for (int i = 0; i < 1000; i++) {
        uint64_t this_size = random() % (random() % 16 ? (size < 32 ? size : 32) : size);
        uint64_t offset = random() % (size-this_size);

        test( dev->read_bytes(dev, offset, this_size, partial2) );
        if ( this_size ) {
            test( memcmp(partial2, partial+offset, this_size) == 0 );

            for (int j = offset; j < offset+this_size; j++)
                partial[j] = random() & 0xFF;

            test( dev->write_bytes(dev, offset, this_size, partial+offset) );
            test( dev->read_bytes(dev, offset, this_size, partial2) );
            test( memcmp(partial2, partial+offset, this_size) == 0 );
        }
    }

    free(partial);
    free(partial2);
}

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);

    char suite_name[128];

    for (uint64_t bs = 1; bs <= 1024; bs *= 2)
        for (uint64_t blocks = 1; blocks <= 16; blocks *= 2) {
            snprintf(suite_name, 128, "slicing bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            struct bdev *dev = bio_create_malloc(bs, blocks);
            assert(dev);

            for (int i = 0; i < 16; i++) {
                uint64_t start = random() % blocks;
                uint64_t len   = 1+(random() % (blocks-start));
                struct bdev *slice = slice_open(dev, start, len);

                test(slice != NULL);
                if ( slice != NULL ) {
                    test_consistency_bdev(slice);
                    test_byte_interface(slice);
                    slice->close(slice);
                }
            }

            dev->close(dev);
        }

    // TODO: slice-specific testing

    test_exit();
}

