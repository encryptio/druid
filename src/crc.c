#include "crc.h"

uint32_t calc_crc32(uint8_t *ram, uint64_t len) {
    uint32_t val = 0;
    // TODO: implement actual crc
    for (int i = 0; i < len; i++)
        val += ram[i]*(i+1);
    return val;
}

