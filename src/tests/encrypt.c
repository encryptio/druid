#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <err.h>
#include <assert.h>

#include "test.h"
#include "layers/baseio.h"
#include "layers/encrypt.h"

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

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);

    char suite_name[128];

    for (uint64_t bs = 32; bs <= 8192; bs *= 2) {
        snprintf(suite_name, 128, "encrypt bs=%d", (int)bs);
        suite(suite_name);

        struct bdev *dev = bio_create_malloc(bs, 32);
        assert(dev != NULL);

        int keylen = random() % 56;
        uint8_t key[56];
        for (int i = 0; i < keylen; i++)
            key[i] = random() % 0xFF;

        test( encrypt_create(dev, key, keylen) );

        struct bdev *enc = encrypt_open(dev, key, keylen);

        test(enc != NULL);
        if ( enc ) {
            test_consistency_bdev(enc);
            enc->close(enc);
        }
    }

    test_exit();
}

