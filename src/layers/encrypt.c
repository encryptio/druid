#include "layers/encrypt.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <err.h>

#include "endian-fns.h"

// using BF_* instead of EVP_* because we need to change the IV quickly
#include <openssl/blowfish.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#define MAGIC "ENCR0000"

/*
 * on-device format:
 *
 * header block:
 *     magic number "ENCR0000"
 *     uint32_t cipher and strengthening to be used
 *         0 -> blowfish in ofb64 mode, with iv=blockindex^baseiv,
 *              strengthening by iterated sha1 and md5 xoring
 *     if blowfish:
 *         8-bytes key verification number
 *         8-bytes baseiv, encrypted in ecb mode
 * 
 * note the blockindex as used above is relative to the projected device
 * start. that is, the second block in the base device which represents
 * the first block of encrypted data has blockindex=0. blockindex is also
 * always big-endian when used to create an iv.
 *
 * TODO: implement XTS cipher mode and make it the default. ofb has a bad
 * leak when used for changing data given the same iv.
 *
 * TODO: add key salting
 */

struct enc_io {
    struct bdev *base;
    uint8_t baseiv[8];
    BF_KEY bf;
    uint8_t *cryptobuffer;
};

// out is a pointer to a 56-byte chunk of memory
static void strengthen_key(uint8_t *key, int keylen, uint8_t *out) {
    assert(keylen <= 56);

    // zero pad into *out
    memset(out, 0, 56);
    memcpy(out, key, keylen);

    uint8_t hash[20];
    for (int i = 0; i < 100000; i++) {
        // add the sha1 hash to the out array by xor,
        // shifting its location on each iteration

        SHA1(out, 56, hash);

        for (int j = i % 56, ct = 0;
                ct < 20;
                j = (j+1) % 56, ct++)
            out[j] ^= hash[ct];

        // same for MD5

        MD5(out, 56, hash);

        for (int j = i % 56, ct = 0;
                ct < 16;
                j = (j+1) % 56, ct++)
            out[j] ^= hash[ct];
    }
}

static void make_key_verification(BF_KEY *bf, uint8_t *into) {
    uint8_t out[8], in[8];
    memset(out, 0, 8);
    memset(into, 0, 8);

    int i = 0;
    for (int j = 0; j < 2000; j++) {
        i += j;

        pack_be64(i, in);
        BF_ecb_encrypt(in, out, bf, BF_ENCRYPT);

        for (int j = 0; j < 8; j++)
            into[j] ^= out[j];
    }
}

bool encrypt_create(struct bdev *dev, uint8_t *key, int keylen) {
    assert(dev->block_size >= 28);

    //////
    // encrypted random baseiv

    BF_KEY bf;
    uint8_t skey[56];
    strengthen_key(key, keylen, skey);
    BF_set_key(&bf, 56, skey);

    uint8_t baseiv[8], baseiv_encrypted[8];
    for (int i = 0; i < 8; i++)
        baseiv[i] = random() & 0xFF;

    BF_ecb_encrypt(baseiv, baseiv_encrypted, &bf, BF_ENCRYPT);

    //////
    // prepare the header block

    uint8_t *block;
    if ( (block = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for temporary block");
    memset(block, 0, dev->block_size);

    memcpy(block, MAGIC, 8);
    pack_be32(0, block+8); // blowfish ofb64 mode

    uint8_t kv[8];
    make_key_verification(&bf, kv);
    memcpy(block+12, kv, 8);

    memcpy(block+20, baseiv_encrypted, 8);

    //////
    // finally, write it out

    if ( !dev->write_block(dev, 0, block) ) {
        fprintf(stderr, "[encrypt] couldn't write header block\n");
        return false;
    }
    
    // the rest of the blocks have implied data

    return true;
}

static bool encrypt_read_block(struct bdev *self, uint64_t which, uint8_t *into) {
    assert(which < self->block_count);

    // TODO: look into doing crypto in-place
    struct enc_io *io = self->m;
    if ( !io->base->read_block(io->base, which+1, io->cryptobuffer) )
        return false;

    uint8_t iv[8];
    pack_be64(which, iv);
    for (int i = 0; i < 8; i++)
        iv[i] ^= io->baseiv[i];

    int num = 0;
    BF_ofb64_encrypt(io->cryptobuffer, into, self->block_size, &(io->bf), iv, &num);

    return true;
}

static bool encrypt_write_block(struct bdev *self, uint64_t which, uint8_t *from) {
    assert(which < self->block_count);

    // TODO: look into doing crypto in-place
    struct enc_io *io = self->m;

    uint8_t iv[8];
    pack_be64(which, iv);
    for (int i = 0; i < 8; i++)
        iv[i] ^= io->baseiv[i];

    int num = 0;
    BF_ofb64_encrypt(from, io->cryptobuffer, self->block_size, &(io->bf), iv, &num);

    return io->base->write_block(io->base, which+1, io->cryptobuffer);
}

static void encrypt_close(struct bdev *self) {
    struct enc_io *io = self->m;
    free(io->cryptobuffer);
    free(io);
    free(self->generic_block_buffer);
    free(self);
}

static void encrypt_clear_caches(struct bdev *self) {
    struct enc_io *io = self->m;
    io->base->clear_caches(io->base);
}

static void encrypt_flush(struct bdev *self) {
    struct enc_io *io = self->m;
    io->base->flush(io->base);
}

struct bdev *encrypt_open(struct bdev *base, uint8_t *key, int keylen) {
    assert(base->block_size >= 28);

    struct bdev *dev;
    if ( (dev = malloc(sizeof(struct bdev))) == NULL )
        err(1, "Couldn't allocate space for bdev encrypt");

    struct enc_io *io;
    if ( (io = malloc(sizeof(struct enc_io))) == NULL )
        err(1, "Couldn't allocate space for bdev encrypt:io");

    dev->m = io;
    dev->block_size = base->block_size;
    dev->block_count = base->block_count-1;

    dev->read_block = encrypt_read_block;
    dev->write_block = encrypt_write_block;
    dev->close = encrypt_close;
    dev->clear_caches = encrypt_clear_caches;
    dev->flush = encrypt_flush;

    dev->read_bytes = generic_read_bytes;
    dev->write_bytes = generic_write_bytes;
    if ( (dev->generic_block_buffer = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for encrypt:block_buffer");
    uint8_t *t = dev->generic_block_buffer;

    if ( (io->cryptobuffer = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for encrypt:cryptobuffer");

    io->base = base;

    uint8_t skey[56];
    strengthen_key(key, keylen, skey);
    BF_set_key(&(io->bf), 56, skey);

    if ( !base->read_block(base, 0, t) )
        goto BAD_END;

    if ( memcmp(t, MAGIC, 8) != 0 ) {
        fprintf(stderr, "[encrypt] Bad magic number\n");
        goto BAD_END;
    }

    uint32_t mode = unpack_be32(t+8);
    if ( mode != 0 ) {
        fprintf(stderr, "[encrypt] unsupported encryption mode\n");
        goto BAD_END;
    }

    uint8_t kv[8];
    make_key_verification(&(io->bf), kv);
    if ( memcmp(t+12, kv, 8) != 0 ) {
        fprintf(stderr, "[encrypt] key verification failed\n");
        goto BAD_END;
    }

    BF_ecb_encrypt(t+20, io->baseiv, &(io->bf), BF_DECRYPT);

    return dev;

BAD_END:
    free(io);
    free(dev->generic_block_buffer);
    free(dev);
    return NULL;
}

