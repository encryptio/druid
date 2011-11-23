#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#include "galois_field.h"
#include "test.h"

#define ALL1(block) \
    for (uint8_t a = 0; ; a++) { \
        block \
        if ( a == 255 ) break; \
    }

#define ALL2(block) \
    for (uint8_t a = 0; ; a++) { \
        for (uint8_t b = 0; ; b++) { \
            block \
            if ( b == 255 ) break; \
        } \
        if ( a == 255 ) break; \
    }

#define SOME3(block) \
    for (int_fast16_t i = 0; i < 50000; i++) {\
        uint8_t a = random(); \
        uint8_t b = random(); \
        uint8_t c = random(); \
        block \
    }

void test_add_commutes() {
    suite("addition commutes");
    ALL2( test(gf_add(a,b) == gf_add(b,a)); )
}

void test_add_associates() {
    suite("addition associates");
    SOME3( test(gf_add(gf_add(a,b), c) == gf_add(a, gf_add(b,c))); )
}

void test_mult_commutes() {
    suite("multiplication commutes");
    ALL2( test(gf_mult(a,b) == gf_mult(b,a)); )
}

void test_mult_associates() {
    suite("multiplication associates");
    SOME3( test(gf_mult(gf_mult(a,b), c) == gf_mult(a, gf_mult(b,c))); )
}

void test_mult_zeroes() {
    suite("multiplication by zero is zero");
    ALL1( test(gf_mult(a,0) == 0); )
}

void test_distributive_law() {
    suite("distributive law");
    SOME3( test(gf_mult(gf_add(a,b), c) == gf_add(gf_mult(a,c), gf_mult(b,c))); )
}

void test_division_inverse() {
    suite("division has nonequal inverse");
    ALL1(
            switch ( a ) {
                case 0:
                case 1:
                    break;

                default:
                    test(gf_div(1,a) != a);
            }
        )
}

int main(int argc, char **argv) {
    srandom(time(NULL));
    test_initialize(argc, argv);
    gf_initialize();

    test_add_commutes();
    test_add_associates();
    test_mult_commutes();
    test_mult_associates();
    test_mult_zeroes();
    test_distributive_law();
    test_division_inverse();

    test_exit();
}

