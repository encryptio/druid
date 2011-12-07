#ifndef __BITVECTOR_H__
#define __BITVECTOR_H__

#include <inttypes.h>

static inline void bit_set(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    set[byte] |= bit;
}

static inline bool bit_get(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    return (set[byte] & bit) ? true : false;
}

static inline void bit_clear(uint8_t *set, uint64_t which) {
    uint64_t byte = which / 8;
    uint8_t bit = 1 << (which % 8);
    set[byte] &= ~bit;
}

static inline uint32_t bit_count_in_u32(uint32_t v) {
    // from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    return ((v + ((v >> 4) & 0xF0F0F0F0)) * 0x10101010) >> 24;
}

#endif
