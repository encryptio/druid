#ifndef __GALOIS_FIELD_H__
#define __GALOIS_FIELD_H__

#include <inttypes.h>

uint8_t gf_add(uint8_t l, uint8_t r);
uint8_t gf_mult(uint8_t l, uint8_t r);
uint8_t gf_div(uint8_t l, uint8_t r);
void gf_initialize();

#endif

