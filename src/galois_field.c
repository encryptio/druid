#include <inttypes.h>

// TODO: look into fecpp

#include "galois_field.h"

#define NW 256
#define PRIM_POLY 0435

static uint8_t gflog[NW];
static uint8_t gfilog[NW];

uint8_t gf_add(uint8_t l, uint8_t r) {
    return l^r;
}

uint8_t gf_mult(uint8_t l, uint8_t r) {
    if ( l == 0 || r == 0 )
        return 0;

    int_fast16_t sum_log = ((int_fast16_t) gflog[l]) + gflog[r];
    if ( sum_log >= NW-1 ) sum_log -= NW-1;

    return gfilog[sum_log];
}

uint8_t gf_div(uint8_t l, uint8_t r) {
    if ( l == 0 ) return 0;
    if ( r == 0 ) return 0; // divide by zero... call error?
    
    int_fast16_t diff_log = ((int_fast16_t) gflog[l]) - gflog[r];
    if ( diff_log < 0 ) diff_log += NW-1;

    return gfilog[diff_log];
}

void gf_initialize() {
    int_fast16_t b = 1;
    for (int log = 0; log < NW-1; log++) {
        gflog[b] = log;
        gfilog[log] = b;
        b <<= 1;
        if ( b & NW ) b ^= PRIM_POLY;
    }
}

