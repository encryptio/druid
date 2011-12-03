#ifndef __CRC_H__
#define __CRC_H__

#include <inttypes.h>

uint32_t calc_crc32(uint8_t *ram, uint64_t len);

#endif
