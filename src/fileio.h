#ifndef __FILEIO_H__
#define __FILEIO_H__

#include <inttypes.h>
#include <stdbool.h>

enum fio_type {
    fio_memory,
    fio_regular_file,
    fio_blockdevice,
    fio_nbd
};

struct fioh {
    char *target;
    enum fio_type type;
    uint64_t size;
    union {
        uint8_t *mem; // data
        int file; // file descriptor
        struct {
        } bd;
        struct {
        } nbd;
    } d;
};

bool fio_read (struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf);
bool fio_write(struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf);
bool fio_commit(struct fioh *h);

uint64_t fio_size_limit(struct fioh *h);
struct fioh * fio_open(char *target);

#endif

