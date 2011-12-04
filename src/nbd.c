#include "nbd.h"

#include "endian-fns.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define NBD_PASSWD "NBDMAGIC"
#define NBD_INITMAGIC "\x00\x00\x42\x02\x81\x86\x12\x53"
#define NBD_REQUEST_MAGIC "\x25\x60\x95\x13"
#define NBD_RESPONSE_MAGIC "\x67\x44\x66\x98"

#define NBD_CMD_READ 0
#define NBD_CMD_WRITE 1
#define NBD_CMD_DISC 2

struct nbd_server * nbd_create(uint16_t port, struct bdev *dev) {
    struct nbd_server *nbd;
    if ( (nbd = malloc(sizeof(struct nbd_server))) == NULL )
        err(1, "Couldn't allocate space for ndb_server");

    nbd->dev = dev;
    nbd->port = port;

    return nbd;
}

static bool nbd_sendall(int s, uint8_t *msg, int len) {
    while ( len > 0 ) {
        int ret = send(s, msg, len, 0);

        if ( ret <= 0 )
            return false; // closed or error

        len -= ret;
        msg += ret;
    }

    return true;
}

static bool nbd_recvall(int s, uint8_t *msg, int len) {
    while ( len > 0 ) {
        int ret = recv(s, msg, len, 0);

        if ( ret <= 0 )
            return false; // closed or error

        len -= ret;
        msg += ret;
    }

    return true;
}

static uint8_t databuffer[128*1024];
static void nbd_handle_client_socket(struct nbd_server *nbd, int s) {
    struct bdev *dev = nbd->dev;
    fprintf(stderr, "[nbd] sending preamble\n");
    memcpy(databuffer, NBD_PASSWD, 8);
    memcpy(databuffer+8, NBD_INITMAGIC, 8);
    pack_be64(dev->block_size*dev->block_count, databuffer+16);
    memset(databuffer+24, 0, 128);
    if ( !nbd_sendall(s, databuffer, 24+128) ) return;
    
    uint8_t buffer[8];
    uint8_t handle[8];
    while ( true ) {
        if ( !nbd_recvall(s, buffer, 8) ) return;
        if ( memcmp(buffer, (void*) NBD_REQUEST_MAGIC, 4) != 0 ) return;

        uint32_t cmd = unpack_be32(buffer+4);

        if ( cmd == NBD_CMD_DISC )
            return;

        if ( !nbd_recvall(s, handle, 8) ) return;

        if ( !nbd_recvall(s, buffer, 8) ) return;
        uint64_t from = unpack_be64(buffer);

        if ( !nbd_recvall(s, buffer, 4) ) return;
        uint32_t len = unpack_be32(buffer);

        if ( len > 128*1024 ) return;

        bool res;
        switch ( cmd ) {
            case NBD_CMD_READ:
                fprintf(stderr, "[nbd] read %llu bytes at %llu\n", (unsigned long long)len, (unsigned long long)from);
                res = dev->read_bytes(dev, from, len, databuffer);

                memcpy(buffer, (void*) NBD_RESPONSE_MAGIC, 4);
                pack_be32((uint32_t) !res, buffer+4);
                if ( !nbd_sendall(s, buffer, 8) ) return;
                if ( !nbd_sendall(s, handle, 8) ) return;

                if ( res )
                    if ( !nbd_sendall(s, databuffer, len) )
                        return;

                break;

            case NBD_CMD_WRITE:
                fprintf(stderr, "[nbd] write %llu bytes at %llu\n", (unsigned long long)len, (unsigned long long)from);
                if ( !nbd_recvall(s, databuffer, len) ) return;

                res = dev->write_bytes(dev, from, len, databuffer);

                memcpy(buffer, (void*) NBD_RESPONSE_MAGIC, 4);
                pack_be32((uint32_t) !res, buffer+4);
                if ( !nbd_sendall(s, buffer, 8) ) return;
                if ( !nbd_sendall(s, handle, 8) ) return;

                break;

            default:
                fprintf(stderr, "PROTOCOL ERROR: unknown command %u\n", cmd);
                return;
        }
    }
}

void nbd_listenloop(struct nbd_server *nbd) {
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // automatically figure out listening address

    char portstr[7];
    snprintf(portstr, 7, "%d", (int)nbd->port);

    if ( (status = getaddrinfo(NULL, portstr, &hints, &servinfo)) < 0 )
        errx(1, "Couldn't getaddrinfo(): %s", gai_strerror(status));

    int servfd;

    if ( (servfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0 )
        err(1, "Couldn't socket()");

    int yes = 1;
    if ( setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0 )
        err(1, "Couldn't setsockopt(SO_REUSEADDR)");

    if ( bind(servfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0 )
        err(1, "Couldn't bind()");

    if ( listen(servfd, 1) < 0 )
        err(1, "Couldn't listen()");

    fprintf(stderr, "[nbd] listening on port %d\n", (int)nbd->port);

    struct sockaddr_storage their_addr;
    socklen_t their_addr_len = sizeof(struct sockaddr_storage);
    int clientfd;
    while ( (clientfd = accept(servfd, (struct sockaddr *restrict) &their_addr, &their_addr_len)) >= 0 ) {
        fprintf(stderr, "[nbd] got client connection\n");
        nbd_handle_client_socket(nbd, clientfd);
        close(clientfd);
        nbd->dev->flush(nbd->dev);
        nbd->dev->clear_caches(nbd->dev);
        fprintf(stderr, "[nbd] closed client connection\n");
    }

    freeaddrinfo(servinfo);
}

