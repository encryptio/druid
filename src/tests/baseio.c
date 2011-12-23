#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <err.h>

#include "test.h"
#include "layers/baseio.h"

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

    for (uint64_t bs = 1; bs <= 8192; bs *= 2)
        for (uint64_t blocks = 4; blocks <= 16; blocks *= 2) {
            snprintf(suite_name, 128, "bio ram bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            struct bdev *dev = bio_create_malloc(bs, blocks);

            test(dev != NULL);
            if ( dev ) {
                test_consistency_bdev(dev);
                test_byte_interface(dev);
                dev->close(dev);
            }

            //////////////////

            snprintf(suite_name, 128, "bio mmap bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            int fd = open("/tmp/druid-test-datastore", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
            if ( fd == -1 )
                err(1, "Couldn't open /tmp/druid-test-datastore");

            if ( unlink("/tmp/druid-test-datastore") )
                err(1, "Couldn't unlink /tmp/druid-test-datastore");

            if ( ftruncate(fd, bs*blocks) )
                err(1, "Couldn't ftruncate /tmp/druid-test-datastore");

            dev = bio_create_mmap(bs, fd, blocks, 0, false, NULL);

            test(dev != NULL);
            if ( dev ) {
                test_consistency_bdev(dev);
                test_byte_interface(dev);
                dev->close(dev);
            }

            //////////////////

            snprintf(suite_name, 128, "bio posixfd bs=%d ct=%d", (int)bs, (int)blocks);
            suite(suite_name);

            dev = bio_create_posixfd(bs, fd, blocks, 0, false, NULL);

            test(dev != NULL);
            if ( dev ) {
                test_consistency_bdev(dev);
                test_byte_interface(dev);
                dev->close(dev);
            }

            close(fd);
        }

    test_exit();
}

