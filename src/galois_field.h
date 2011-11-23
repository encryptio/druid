#ifndef __GALOIS_FIELD_H__
#define __GALOIS_FIELD_H__

#include <inttypes.h>

#define GF_NW 256
#define GF_PRIM_POLY 0435

extern uint8_t gf_log[GF_NW];
extern uint8_t gf_ilog[GF_NW];

static inline uint8_t gf_add(uint8_t l, uint8_t r) {
    return l^r;
}

static inline uint8_t gf_mult(uint8_t l, uint8_t r) {
    if ( l == 0 || r == 0 )
        return 0;

    int_fast16_t sum_log = ((int_fast16_t) gf_log[l]) + gf_log[r];
    if ( sum_log >= GF_NW-1 ) sum_log -= GF_NW-1;

    return gf_ilog[sum_log];
}

static inline uint8_t gf_div(uint8_t l, uint8_t r) {
    if ( l == 0 ) return 0;
    if ( r == 0 ) return 0; // divide by zero... call error?
    
    int_fast16_t diff_log = ((int_fast16_t) gf_log[l]) - gf_log[r];
    if ( diff_log < 0 ) diff_log += GF_NW-1;

    return gf_ilog[diff_log];
}

void gf_initialize();

#endif

