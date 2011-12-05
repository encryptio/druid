#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <err.h>
#include <assert.h>

#include "test.h"
#include "baseio.h"
#include "partitioner.h"

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);

    suite("partitioner");

    // block size 1024 bytes
    // 16384 blocks

    struct bdev *dev = bio_create_malloc(1024, 16384);
    assert(dev);

    assert(partitioner_initialize(dev));

    uint8_t part_contents[16][64];
    int part_sizes[16];

    memset(part_sizes, 0, sizeof(int)*16);
    memset(part_contents, 0, 16*64);

    uint8_t block_contents[1024];

    memset(block_contents, 0, 1024);

    for (int i = 0; i < 100000; i++) {
        int part = random() % 4;
        int block = random() % 64;
        uint8_t set_to = random() % 256;

        struct bdev *p = NULL;

        switch ( random() % 3 ) {
            case 0:
                // resize test
                fprintf(stderr, "resize %d -> %d\n", part, block+1);
                if ( part_sizes[part] > block+1 ) {
                    fprintf(stderr, "  skipping, would shrink\n");
                } else {
                    test(partitioner_set_part_size(dev, part, block+1));
                    for (int i = part_sizes[part]; i < block+1; i++)
                        part_contents[part][i] = 0x00;
                    part_sizes[part] = block+1;
                }
                break;

            case 1:
                // read test
                if ( block < part_sizes[part] ) {
                    fprintf(stderr, "read %d.%d\n", part, block);
                    p = partitioner_open(dev, part);
                    test(p != NULL);

                    test(p->read_block(p, block, block_contents));
                    fprintf(stderr, "  got %d, expected %d\n", (int)block_contents[0], part_contents[part][block]);
                    test(block_contents[0] == part_contents[part][block]);
                    test(block_contents[1] == 0);

                    p->close(p);
                    p = NULL;
                }
                break;

            case 2:
                // write test
                if ( block < part_sizes[part] ) {
                    fprintf(stderr, "write %d.%d <- %d\n", part, block, (int)set_to);
                    p = partitioner_open(dev, part);
                    test(p != NULL);

                    block_contents[0] = set_to;
                    block_contents[1] = 0;

                    test(p->write_block(p, block, block_contents));
                    part_contents[part][block] = set_to;
                    
                    p->close(p);
                    p = NULL;
                }
                break;
        }
    }

    test_exit();
}

