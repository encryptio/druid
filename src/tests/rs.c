#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#include "rs/reed_solomon.h"
#include "rs/galois_field.h"
#include "test.h"

void rs_test_run(int n, int m) {
    rs_matrix *mat = rs_create(n,m);

    uint8_t *data = calloc(1, n+m);
    for (int i = 0; i < n; i++)
        data[i] = random();

    uint8_t *cs = data+n;
    uint8_t *exists = calloc(1, n+m+1);

    uint8_t *stuff = calloc(1, n);
    uint8_t *newstuff = calloc(1, n);

    rs_apply_checksum(mat, data, cs);

    for (;;) {
        // increment "exists"
        int i = 0;
        while ( 1 < ++exists[i] ) {
            exists[i] = 0;
            if ( n+m < ++i ) goto DONE;
        }

        // only continue if exactly n items in "exists" are set
        int on = 0;
        for (i = 0; i < n+m; i++)
            if ( exists[i] )
                on++;
        if ( on != n ) continue;

        rs_matrix *rev = rs_create_reversal(mat, exists);

        int j = 0;
        for (i = 0; i < n+m; i++)
            if ( exists[i] )
                stuff[j++] = data[i];

        rs_apply_reversal(rev, stuff, newstuff);

        for (int i = 0; i < n; i++)
            test(newstuff[i] == data[i]);
    }
DONE:

    free(newstuff);
    free(stuff);
    free(data);
    free(exists);
    rs_free(mat);
}

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);
    gf_initialize();

    suite("reed solomon error correction");

    for (int n = 2; n <= 8; n++)
        for (int m = 1; m <= 8; m++)
            rs_test_run(n, m);

    test_exit();
}

