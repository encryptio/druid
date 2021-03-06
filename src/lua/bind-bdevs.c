#include "lua/bind.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "bdev.h"
#include "logger.h"

struct bdev_data {
    struct bdev *dev;

    // references to other bdevs
    int *refs;
    int ref_count;
};

static uint64_t luaL_checkuint64(lua_State *L, int index) {
    lua_Number v = luaL_checknumber(L, index);
    luaL_argcheck(L, (fabs(v-round(v)) < 0.000001), index, "is not an integer");
    return v;
}

////////////////////////////////////////////////////////////////////////////////

static int bind_bdev_get_name(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    lua_pushstring(L, bdd->dev->name);

    return 1;
}

static int bind_bdev_get_block_size(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    lua_pushnumber(L, bdd->dev->block_size);

    return 1;
}

static int bind_bdev_get_block_count(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    lua_pushnumber(L, bdd->dev->block_count);

    return 1;
}

static int bind_bdev_read_bytes(lua_State *L) {
    require_exactly(L, 3);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    uint64_t start        = luaL_checkuint64(L, 2);
    uint64_t len          = luaL_checkuint64(L, 3);

    uint64_t size = bdd->dev->block_count * bdd->dev->block_size;

    luaL_argcheck(L, (luaL_checknumber(L,2) >= 0), 2, "start cannot be negative");
    luaL_argcheck(L, (luaL_checknumber(L,3) >= 0), 3, "length cannot be negative");
    luaL_argcheck(L, (start < size && start+len <= size), 2, "Can't read past the end of the device");
    luaL_argcheck(L, (len > 0), 3, "length cannot be zero");
    lua_pop(L, 3);

    uint8_t *data;
    if ( (data = malloc(len)) == NULL )
        err(1, "Couldn't allocate space for data read");

    if ( bdd->dev->read_bytes(bdd->dev, start, len, data) )
        lua_pushlstring(L, (char*)data, len);
    else
        lua_pushnil(L);

    free(data);

    return 1;
}

static int bind_bdev_write_bytes(lua_State *L) {
    require_exactly(L, 3);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    uint64_t start      = luaL_checkuint64(L, 2);
    size_t len;
    luaL_checktype(L, 3, LUA_TSTRING);
    const uint8_t *data = (const uint8_t *) luaL_checklstring(L, 3, &len);

    uint64_t size = bdd->dev->block_count * bdd->dev->block_size;

    luaL_argcheck(L, (luaL_checknumber(L,2) >= 0), 2, "start cannot be negative");
    luaL_argcheck(L, (start < size && start+len <= size), 2, "Can't write past the end of the device");
    luaL_argcheck(L, (len > 0), 3, "string length cannot be zero");
    lua_pop(L, 3);

    bool ret = bdd->dev->write_bytes(bdd->dev, start, len, data);

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_bdev_read_block(lua_State *L) {
    require_exactly(L, 2);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    uint64_t which        = luaL_checkuint64(L, 2);

    luaL_argcheck(L, (luaL_checknumber(L,2) >= 0), 2, "Can't read before the start of the device");
    luaL_argcheck(L, (which < bdd->dev->block_count), 2, "Can't read after the end of the device");
    lua_pop(L, 2);

    uint8_t *data;
    if ( (data = malloc(bdd->dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for block read");

    if ( bdd->dev->read_block(bdd->dev, which, data) )
        lua_pushlstring(L, (char*)data, bdd->dev->block_size);
    else
        lua_pushnil(L);

    free(data);

    return 1;
}

static int bind_bdev_write_block(lua_State *L) {
    require_exactly(L, 3);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    uint64_t which        = luaL_checkuint64(L, 2);
    size_t len;
    luaL_checktype(L, 3, LUA_TSTRING);
    const uint8_t *data   = (const uint8_t *) luaL_checklstring(L, 3, &len);

    luaL_argcheck(L, (luaL_checknumber(L,2) >= 0), 2, "Can't write before the start of the device");
    luaL_argcheck(L, (which < bdd->dev->block_count), 2, "Can't write after the end of the device");
    luaL_argcheck(L, (len == bdd->dev->block_size), 3, "Can't write a string that isn't the length of the block size");
    lua_pop(L, 3);

    bool ret = bdd->dev->write_block(bdd->dev, which, data);

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_bdev_close(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    if ( bdd->dev )
        bdd->dev->close(bdd->dev);
    bdd->dev = NULL;

    if ( bdd->refs ) {
        for (int i = 0; i < bdd->ref_count; i++)
            luaL_unref(L, LUA_REGISTRYINDEX, bdd->refs[i]);
        free(bdd->refs);
        bdd->ref_count = 0;
    }

    return 0;
}

static int bind_bdev_clear_caches(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    bdd->dev->clear_caches(bdd->dev);

    return 0;
}

static int bind_bdev_flush(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    bdd->dev->flush(bdd->dev);

    return 0;
}

static int bind_bdev_sync(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    bdd->dev->sync(bdd->dev);

    return 0;
}

static int bind_bdev_wrap(lua_State *L, struct bdev *dev, int *refs, int ref_count) {
    if ( dev == NULL ) {
        free(refs);
        lua_pushnil(L);
        return 1;
    }

    struct bdev_data *bdd = lua_newuserdata(L, sizeof(struct bdev_data));
    assert(bdd);
    int userdata = lua_gettop(L);

    bdd->dev = dev;
    bdd->refs = refs;
    bdd->ref_count = ref_count;

    if ( luaL_newmetatable(L, "druid bdev") ) {
        int table = lua_gettop(L);

        luaL_Reg fns[] = {
            { "__gc", bind_bdev_close },
            { "name", bind_bdev_get_name },
            { "block_size", bind_bdev_get_block_size },
            { "block_count", bind_bdev_get_block_count },
            { "read_bytes", bind_bdev_read_bytes },
            { "write_bytes", bind_bdev_write_bytes },
            { "read_block", bind_bdev_read_block },
            { "write_block", bind_bdev_write_block },
            { "clear_caches", bind_bdev_clear_caches },
            { "flush", bind_bdev_flush },
            { "sync", bind_bdev_sync },
            { NULL, NULL }
        };

        luaL_register(L, NULL, fns);

        lua_pushliteral(L, "__index");
        lua_pushvalue(L, table);
        lua_settable(L, table);
    }
    
    lua_setmetatable(L, userdata);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/baseio.h"

static int bind_bio_create_malloc(lua_State *L) {
    require_exactly(L, 2);

    uint64_t block_size = luaL_checkuint64(L, 1);
    size_t blocks       = luaL_checkuint64(L, 2);
    lua_pop(L, 2);

    luaL_argcheck(L, (block_size >= 1),         1, "Unreasonable block size");
    luaL_argcheck(L, (block_size <= (1 << 20)), 1, "Unreasonable block size");
    luaL_argcheck(L, (blocks     >= 1),         2, "Zero block count");

    return bind_bdev_wrap( L, bio_create_malloc(block_size, blocks), NULL, 0 );
}

static int bind_file(lua_State *L) {
    require_atleast(L, 1);

    const char *filename = luaL_checkstring(L, 1);

    bool block_size_set = false;
    uint64_t block_size = 0;

    bool blocks_set = false;
    uint64_t blocks = 0, offset = 0;

    if ( lua_gettop(L) >= 2 ) {
        block_size_set = true;
        block_size = luaL_checkuint64(L, 2);

        if ( lua_gettop(L) >= 3 ) {
            blocks_set = true;
            blocks = luaL_checkuint64(L, 3);

            if ( lua_gettop(L) >= 4 )
                offset = luaL_checkuint64(L, 4);
        }
    }

    lua_pop(L, lua_gettop(L)-1);

    // stack: filename
    
    if ( block_size_set ) {
        luaL_argcheck(L, (block_size >= 1),         1, "Unreasonable block size");
        luaL_argcheck(L, (block_size <= (1 << 20)), 1, "Unreasonable block size");
    }
    if ( blocks_set )
        luaL_argcheck(L, (blocks     >= 1),     2, "Zero block count");

    int fd = open(filename, O_RDWR|(blocks_set ? O_CREAT : 0), S_IRUSR|S_IWUSR);
        

    if ( fd == -1 ) {
        logger(LOG_ERR, "bind", "Couldn't open %s for reading and writing: %s", filename, strerror(errno));

        lua_pop(L, 1);
        // stack: <empty>

        lua_pushnil(L);
        return 1;
    }

    struct stat st;
    int ret = fstat(fd, &st);
    if ( ret < 0 ) {
        logger(LOG_ERR, "bind", "Couldn't fstat(%s): %s", filename, strerror(errno));

        close(fd); // ignore errors

        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }

    if ( block_size_set ) {
        if ( block_size % st.st_blksize )
            logger(LOG_INFO, "bind", "File \"%s\"'s optimal IO size is %d bytes, but the block size requested is %d bytes.", filename, (int)st.st_blksize, (int)block_size);
    } else {
        block_size = st.st_blksize;
        logger(LOG_JUNK, "bind", "Assuming block size of %d for %s", (int)block_size, filename);

        assert(block_size > 0);
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if ( size == -1 ) {
        logger(LOG_ERR, "bind", "File \"%s\" isn't seekable", filename);
        close(fd);
        
        lua_pushnil(L);
        return 1;
    }
    lseek(fd, 0, SEEK_SET);

    if ( blocks_set ) {
        // make sure there are at least that number of blocks in the file

        uint64_t wanted = block_size * blocks + offset;

        if ( size < wanted )
            if ( ftruncate(fd, wanted) < 0 )
                logger(LOG_ERR, "bind", "Couldn't ftruncate(%s) to %llu blocks of %d bytes: %s", filename, blocks, (int)block_size, strerror(errno));
    } else {
        // not blocks_set
        // set the block count to the size of the file
        // note: offset must be 0 in this case

        // TODO: should it be an error to open a file with a block size
        //       that doesn't evenly divide the file size?
        blocks = (size + block_size - 1) / block_size;
    }

    struct bdev *dev;
// XXX: mmap does not report read/write errors, it kills the process. fix that.
#if 0
    // try mmap first
    dev = bio_create_mmap(block_size, fd, blocks, offset, true, filename);
    if ( dev != NULL ) {
        // now try to mmap some more (anonymous) space - this makes sure we
        // actually have enough VM to continue operating reliably

        // TODO: make the mmap size here configurable
        void *extra = mmap(NULL, 1024*1024*64, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if ( extra == MAP_FAILED ) {
            logger(LOG_WARN, "bind", "Tried to mmap extra space after mmaping %s, but failed to: %s. Falling back to posixfd.", filename, strerror(errno));
            
            dev->close(dev); // closes fd, so we need to reopen it
            fd = open(filename, O_RDWR, S_IRUSR|S_IWUSR);
            assert(fd >= 0);

        } else {
            if ( munmap(extra, 1024*1024*64) < 0 )
                err(1, "Couldn't munmap extra space while opening %s", filename);

            return bind_bdev_wrap(L, dev, NULL, 0);
        }
    }
#endif
    // file mmap or extra space test failed

    // baseio takes over responsibility for closing fd if it returns non-null
    dev = bio_create_posixfd(block_size, fd, blocks, offset, true, filename);

    if ( dev == NULL )
        close(fd);

    return bind_bdev_wrap(L, dev, NULL, 0);
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/concat.h"

static int bind_concat_open(lua_State *L) {
    require_atleast(L, 1);

    // TODO: allow argument to be a table of the devices
    // TODO: disallow duplicate devices
    
    int count = lua_gettop(L);

    int *refs;
    if ( (refs = malloc(sizeof(int) * count)) == NULL )
        err(1, "Couldn't allocate space for references list");
    struct bdev **devices;
    if ( (devices = malloc(sizeof(struct bdev *) * count)) == NULL )
        err(1, "Couldn't allocate space for devices list");

    for (int i = 0; i < count; i++) {
        lua_pushvalue(L, i+1);
        struct bdev_data *bdd = luaL_checkudata(L, -1, "druid bdev");
        for (int j = 0; j < i; j++)
            luaL_argcheck(L, (bdd->dev != devices[j]), i+1, "Can't concat a device with itself");
        refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
        devices[i] = bdd->dev;
    }

    lua_pop(L, count);

    struct bdev *dev = concat_open(devices, count);
    free(devices);

    return bind_bdev_wrap(L, dev, refs, count);
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/encrypt.h"

static int bind_encrypt_create(lua_State *L) {
    require_exactly(L, 2);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    size_t keylen;
    const uint8_t *key = (const uint8_t *) luaL_checklstring(L, 2, &keylen);

    bool ret = encrypt_create(bdd->dev, key, keylen);
    lua_pop(L, 2); // can't do this earlier

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_encrypt_open(lua_State *L) {
    require_exactly(L, 2);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    size_t keylen;
    const uint8_t *key = (const uint8_t *) luaL_checklstring(L, 2, &keylen);

    struct bdev *ret = encrypt_open(bdd->dev, key, keylen);

    lua_pop(L, 1);

    int *refs;
    if ( (refs = malloc(sizeof(int))) == NULL )
        err(1, "Couldn't allocate space for references list");
    refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
    
    return bind_bdev_wrap(L, ret, refs, 1);
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/slice.h"

static int bind_slice_open(lua_State *L) {
    require_exactly(L, 3);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    uint64_t start        = luaL_checkuint64(L, 2);
    uint64_t len          = luaL_checkuint64(L, 3);

    luaL_argcheck(L, (luaL_checknumber(L, 2) >= 0), 2, "Can't open a slice to before the start of the device");
    luaL_argcheck(L, (start+len <= bdd->dev->block_count), 2, "Can't open a slice to after the end of the device");
    luaL_argcheck(L, (luaL_checknumber(L, 3) > 0), 3, "Can't open a slice with a non-positive size");

    lua_pop(L, 2);

    int *refs;
    if ( (refs = malloc(sizeof(int))) == NULL )
        err(1, "Couldn't allocate space for references list");
    refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);

    return bind_bdev_wrap( L, slice_open(bdd->dev, start, len), refs, 1 );
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/stripe.h"

static int bind_stripe_open(lua_State *L) {
    require_atleast(L, 1);

    // TODO: allow argument to be a table of the devices
    // TODO: disallow duplicate devices
    
    int count = lua_gettop(L);

    int *refs;
    if ( (refs = malloc(sizeof(int) * count)) == NULL )
        err(1, "Couldn't allocate space for references list");
    struct bdev **devices;
    if ( (devices = malloc(sizeof(struct bdev *) * count)) == NULL )
        err(1, "Couldn't allocate space for devices list");

    for (int i = 0; i < count; i++) {
        lua_pushvalue(L, i+1);
        struct bdev_data *bdd = luaL_checkudata(L, -1, "druid bdev");
        for (int j = 0; j < i; j++)
            luaL_argcheck(L, (bdd->dev != devices[j]), i+1, "Can't concat a device with itself");
        refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
        devices[i] = bdd->dev;
    }

    lua_pop(L, count);

    struct bdev *dev = stripe_open(devices, count);
    free(devices);

    return bind_bdev_wrap(L, dev, refs, count);
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/verify.h"

static int bind_verify_create(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");

    int *refs;
    if ( (refs = malloc(sizeof(int))) == NULL )
        err(1, "Couldn't allocate space for references list");
    refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);

    return bind_bdev_wrap( L, verify_create(bdd->dev), refs, 1 );
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/lazyzero.h"

static int bind_lazyzero_create(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");
    lua_pop(L, 1);

    lua_pushboolean(L, lazyzero_create(bdd->dev));

    return 1;
}

static int bind_lazyzero_open(lua_State *L) {
    require_exactly(L, 1);

    struct bdev_data *bdd = luaL_checkudata(L, 1, "druid bdev");

    int *refs;
    if ( (refs = malloc(sizeof(int))) == NULL )
        err(1, "Couldn't allocate space for references list");
    refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);

    return bind_bdev_wrap( L, lazyzero_open(bdd->dev), refs, 1 );
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/xor.h"

static int bind_xor_open(lua_State *L) {
    require_atleast(L, 3);

    // TODO: allow argument to be a table of the devices
    // TODO: disallow duplicate devices
    
    int count = lua_gettop(L);

    int *refs;
    if ( (refs = malloc(sizeof(int) * count)) == NULL )
        err(1, "Couldn't allocate space for references list");
    struct bdev **devices;
    if ( (devices = malloc(sizeof(struct bdev *) * count)) == NULL )
        err(1, "Couldn't allocate space for devices list");

    for (int i = 0; i < count; i++) {
        lua_pushvalue(L, i+1);
        struct bdev_data *bdd = luaL_checkudata(L, -1, "druid bdev");
        for (int j = 0; j < i; j++)
            luaL_argcheck(L, (bdd->dev != devices[j]), i+1, "Can't concat a device with itself");
        refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
        devices[i] = bdd->dev;
    }

    lua_pop(L, count);

    struct bdev *dev = xor_open(devices, count);
    free(devices);

    return bind_bdev_wrap(L, dev, refs, count);
}

////////////////////////////////////////////////////////////////////////////////

int bind_bdevs(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "ram", bind_bio_create_malloc },

        // TODO
        { "file", bind_file },

        { "concat", bind_concat_open },

        { "encrypt_initialize", bind_encrypt_create },
        { "encrypt", bind_encrypt_open },

        { "slice", bind_slice_open },

        { "stripe", bind_stripe_open },

        { "verify", bind_verify_create },

        { "lazyzero_initialize", bind_lazyzero_create },
        { "lazyzero", bind_lazyzero_open },

        { "xor", bind_xor_open },

        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

