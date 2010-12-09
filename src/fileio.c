#include "fileio.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

bool fio_read (struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf) {
    if ( offset+size > h->size ) {
        fprintf(stderr, "[fileio] ERROR: (%s) tried to read from %llu+%u, which is outside of allowed location 0..%llu\n",
                h->target, offset, size, h->size);
        return false;
    }

    off_t loc;
    switch ( h->type ) {
        case fio_memory:
            memcpy(buf, h->d.mem+offset, size);
            return true;

        case fio_regular_file:
            loc = lseek(h->d.file, offset, SEEK_SET);
            if ( loc < 0 ) {
                fprintf(stderr, "[fileio] ERROR: (%s) unable to seek to %llu: %s\n", h->target, offset, strerror(errno));
                return false;
            }

            while ( size ) {
                ssize_t justread = read(h->d.file, buf, size);
                if ( justread < 0 ) {
                    fprintf(stderr, "[fileio] ERROR: (%s) couldn't read %u bytes: %s\n", h->target, size, strerror(errno));
                    return false;
                }

                size -= justread;
                buf += justread;
            }

            return true;

        default:
            err(1, "Internal error: fio_read(%p) with unknown type %d", h, h->type);
    }
}
bool fio_write(struct fioh *h, uint64_t offset, uint32_t size, uint8_t *buf) {
    if ( offset+size > h->size ) {
        fprintf(stderr, "[fileio] ERROR: (%s) tried to write to %llu+%u, which is outside of allowed location 0..%llu\n",
                h->target, offset, size, h->size);
        return false;
    }

    off_t loc;
    switch ( h->type ) {
        case fio_memory:
            memcpy(h->d.mem+offset, buf, size);
            return true;

        case fio_regular_file:
            loc = lseek(h->d.file, offset, SEEK_SET);
            if ( loc < 0 ) {
                fprintf(stderr, "[fileio] ERROR: (%s) unable to seek to %llu: %s\n", h->target, offset, strerror(errno));
                return false;
            }

            while ( size ) {
                ssize_t written = write(h->d.file, buf, size);
                if ( written < 0 ) {
                    fprintf(stderr, "[fileio] ERROR: (%s) couldn't write %u bytes: %s\n", h->target, size, strerror(errno));
                    return false;
                }

                size -= written;
                buf += written;
            }

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

        case fio_regular_file:
            if ( fsync(h->d.file) ) {
                fprintf(stderr, "[fileio] ERROR: (%s) couldn't fsync. %s", h->target, strerror(errno));
                return false;
            } else {
                return true;
            }

        default:
            err(1, "Internal error: fio_commit(%p) with unknown type %d", h, h->type);
    }
}

uint64_t fio_size_limit(struct fioh *h) {
    return h->size;
}

struct fioh * fio_open(char *target) {
    struct fioh *h;

    if ( (h = malloc(sizeof(struct fioh))) == NULL )
        err(1, "Couldn't allocate memory for fioh");

    h->type = -1;
    h->size = 0;
    h->target = strdup(target);

    int len = strlen(target);
    if ( len > 4 && memcmp(target, "mem:", 4) == 0 ) {
        h->type = fio_memory;
        h->d.mem = NULL;

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
                    goto FAIL;
            }
        }

        h->size = size;

        if ( (h->d.mem = malloc(size)) == NULL ) {
            fprintf(stderr, "[fileio] couldn't allocate space for %s\n", target+4);
            goto FAIL;
        }

        return h;
    } else if ( len > 5 && memcmp(target, "file:", 5) == 0 ) {
        h->type = fio_regular_file;

        if ( (h->d.file = open(target+5, O_RDWR)) < 0 ) {
            fprintf(stderr, "[fileio] couldn't open %s for read/write: %s\n", target+5, strerror(errno));
            goto FAIL;
        }

        off_t size = lseek(h->d.file, 0, SEEK_END);
        if ( size < 0 ) {
            fprintf(stderr, "[fileio] couldn't seek to the end of %s: %s\n", target+5, strerror(errno));
            goto FAIL;
        }

        if ( size == 0 ) {
            fprintf(stderr, "[fileio] size of %s is zero\n", target+5);
            goto FAIL;
        }

        h->size = size;

        return h;

    } else {
        fprintf(stderr, "[fileio] bad target: %s\n", target);
        goto FAIL;
    }

FAIL:
    if ( h->type == fio_memory )
        if ( h->d.mem )
            free(h->d.mem);

    if ( h->type == fio_regular_file )
        if ( h->d.file >= 0 )
            if ( close(h->d.file) )
                fprintf(stderr, "[fileio] couldn't close filehandle\n");

    free(h->target);
    free(h);
    return NULL;
}

