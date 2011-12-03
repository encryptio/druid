#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <err.h>
#include <assert.h>

#include "test.h"
#include "baseio.h"
#include "verify.h" 
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
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < dev->block_size; j++)
            basis[j] = random() & 0xFF;
        test_consistency_bdev_with_block(dev, basis, block);
    }

    free(block);
    free(basis);
}

void test_corruption(struct bdev *dev, struct bdev *basis) {
    uint8_t *block;

    if ( (block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't malloc space for block");

    // first, clear blocks 1-3 in the basis to zeroes so the test is deterministic
    memset(block, 0, dev->block_size);
    test( dev->write_block(dev, 0, block) );
    test( dev->write_block(dev, 1, block) );
    test( dev->write_block(dev, 2, block) );

    if ( dev->clear_caches ) dev->clear_caches(dev);
    if ( dev->flush        ) dev->flush(dev);

    // read block 1-3 in basis (0-2 on dev)
    test( dev->read_block(dev, 0, block) );
    test( dev->read_block(dev, 1, block) );
    test( dev->read_block(dev, 2, block) );

    // corrupt it
    test( basis->read_block(basis, 1, block) );
    block[0] = block[0] ^ 0x40;
    test( basis->write_block(basis, 1, block) );

    if ( dev->clear_caches ) dev->clear_caches(dev);

    // and fail to read it again
    test( !dev->read_block(dev, 0, block) );

    // read block 2 in basis (1 on dev)
    test( dev->read_block(dev, 1, block) );

    // corrupt block 0 in basis (hash block)
    test( basis->read_block(basis, 0, block) );
    for (int i = 0; i < basis->block_size; i++)
        if ( i % 4 == 1 )
            block[i] = block[i] ^ 0x01;
    test( basis->write_block(basis, 0, block) );

    if ( dev->clear_caches ) dev->clear_caches(dev);

    // fail to read blocks 1-3 in basis (0-2 on dev)
    test( !dev->read_block(dev, 0, block) );
    test( !dev->read_block(dev, 1, block) );
    test( !dev->read_block(dev, 2, block) );
}

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);

    char suite_name[128];

    for (uint64_t bs = 16; bs <= 16384; bs *= 2)
        for (uint64_t blocks = 4; blocks <= 131072; blocks *= 2) {
            if ( bs*blocks > 1024ULL*1024*2 ) {
                // skip tests that will use more than 2 megs of ram
                continue;
            }

            snprintf(suite_name, 128, "verify creation bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            struct bdev *base = bio_create_malloc(bs, blocks);
            assert(base != NULL);

            struct bdev *ver = verify_create(base);

            test(ver != NULL);
            if ( ver ) {
                snprintf(suite_name, 128, "verify consistency bs=%d ct=%d", (int)bs, (int)blocks);
                suite(suite_name);
                test_consistency_bdev(ver);

                snprintf(suite_name, 128, "verify corruption bs=%d ct=%d", (int)bs, (int)blocks);
                suite(suite_name);
                test_corruption(ver, base);

                ver->close(ver);
            }

            base->close(base);
        }

    test_exit();
}

