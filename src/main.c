#include "layers/nbd.h"
#include "layers/baseio.h"
#include "layers/verify.h"
#include "layers/partitioner.h"
#include "layers/encrypt.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <err.h>

int main(void) {
    int fd = open("data-store", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if ( fd == -1 )
        err(1, "Couldn't open data-store");
    if ( ftruncate(fd, 1024*1024*1024) ) // 1 GiB
        err(1, "Couldn't ftruncate data-store");

    struct bdev *base = bio_create_mmap(1024, fd, 1024*1024, 0);
    assert(base);

    char *key = "hellisotherpeople";

    encrypt_create(base, (uint8_t*)key, 17);

    struct bdev *p = encrypt_open(base, (uint8_t*)key, 17);

    struct nbd_server *nbd = nbd_create(1234, p);
    assert(nbd);
    nbd_listenloop(nbd);
}

