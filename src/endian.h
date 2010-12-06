#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include <inttypes.h>

static inline void pack_be64(uint64_t from, uint8_t *to) {
    to[7] = from & 0xFF; from >>= 8;
    to[6] = from & 0xFF; from >>= 8;
    to[5] = from & 0xFF; from >>= 8;
    to[4] = from & 0xFF; from >>= 8;
    to[3] = from & 0xFF; from >>= 8;
    to[2] = from & 0xFF; from >>= 8;
    to[1] = from & 0xFF; from >>= 8;
    to[0] = from & 0xFF;
}

static inline void pack_be32(uint64_t from, uint8_t *to) {
    to[3] = from & 0xFF; from >>= 8;
    to[2] = from & 0xFF; from >>= 8;
    to[1] = from & 0xFF; from >>= 8;
    to[0] = from & 0xFF;
}

static inline uint64_t unpack_be64(uint8_t *from) {
    return (((((((((((uint64_t) from[0]) << 8)) +
                   (((uint64_t) from[1]) << 8)) +
                   (((uint64_t) from[2]) << 8)) +
                   (((uint64_t) from[3]) << 8)) +
                   (((uint64_t) from[4]) << 8)) +
                   (((uint64_t) from[5]) << 8)) +
                   (((uint64_t) from[6]) << 8)) +
                   (((uint64_t) from[7]) << 8));
}

static inline uint32_t unpack_be32(uint8_t *from) {
    return (((((((uint32_t) from[0]) << 8)) +
               (((uint32_t) from[1]) << 8)) +
               (((uint32_t) from[2]) << 8)) +
               (((uint32_t) from[3]) << 8));
                   
                   
                   
                   
}

#endif
