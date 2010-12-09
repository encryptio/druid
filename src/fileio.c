#include "fileio.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

bool fio_read (struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf) {
    switch ( h->type ) {
        case fio_memory:
            if ( offset+size > h->d.mem.size ) {
                fprintf(stderr, "[fileio] tried to read from %llu+%u, which is outside of allowed location 0..%llu\n",
                        offset, size, h->d.mem.size);
                return false;
            }

            memcpy(buf, h->d.mem.data+offset, size);
            return true;

        default:
            err(1, "Internal error: fio_read(%p) with unknown type %d", h, h->type);
    }
}
bool fio_write(struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf) {
    switch ( h->type ) {
        case fio_memory:
            if ( offset+size > h->d.mem.size ) {
                fprintf(stderr, "[fileio] tried to write to %llu+%u, which is outside of allowed location 0..%llu\n",
                        offset, size, h->d.mem.size);
                return false;
            }

            memcpy(h->d.mem.data+offset, buf, size);
            return true;

        default:
            err(1, "Internal error: fio_write(%p) with unknown type %d", h, h->type);
    }
}

bool fio_commit(struct fioh *h) {
    switch ( h->type ) {
        case fio_memory:
            // do nothing
            return true;

        default:
            err(1, "Internal error: fio_commit(%p) with unknown type %d", h, h->type);
    }
}

uint64_t fio_size_limit(struct fioh *h) {
    switch ( h->type ) {
        case fio_memory:
            return h->d.mem.size;

        default:
            err(1, "Internal error: fio_size_limit(%p) with unknown type %d", h, h->type);
    }
}

struct fioh * fio_open(char *target) {
    struct fioh *fioh;

    if ( (fioh = malloc(sizeof(struct fioh))) == NULL )
        err(1, "Couldn't allocate memory for fioh");

    int len = strlen(target);
    if ( len > 4 && memcmp(target, "mem:", 4) == 0 ) {
        uint64_t size = 0;

        char *t = target+4;
        while ( *t >= '0' && *t <= '9' ) {
            size = size * 10 + (*t - '0');
            t++;
        }

        if ( *t ) {
            switch ( *t ) {
                case 't':
                case 'T':
                    size *= 1024;

                case 'g':
                case 'G':
                    size *= 1024;

                case 'm':
                case 'M':
                    size *= 1024;

                case 'k':
                case 'K':
                    size *= 1024;
                    break;

                default:
                    fprintf(stderr, "[fileio] bad spec for memory, trailing garbage\n");
                    free(fioh);
                    return NULL;
            }
        }

        fioh->type = fio_memory;
        fioh->d.mem.size = size;

        if ( (fioh->d.mem.data = malloc(size)) == NULL ) {
            fprintf(stderr, "[fileio] couldn't allocate space for %s\n", target+4);
            free(fioh);
            return NULL;
        }

        return fioh;
    } else {
        fprintf(stderr, "[fileio] bad target: %s\n", target);
        free(fioh);
        return NULL;
    }
}

