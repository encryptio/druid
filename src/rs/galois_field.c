#include <inttypes.h>

// TODO: look into fecpp

#include "galois_field.h"

uint8_t gf_log[GF_NW];
uint8_t gf_ilog[GF_NW];

void gf_initialize() {
    int_fast16_t b = 1;
    for (int log = 0; log < GF_NW-1; log++) {
        gf_log[b] = log;
        gf_ilog[log] = b;
        b <<= 1;
        if ( b & GF_NW ) b ^= GF_PRIM_POLY;
    }
}

