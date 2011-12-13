#include "lua/bind.h"

#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
#include "bdev.h"

static int bind_bdev_get_block_size(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    lua_pushnumber(L, dev->block_size);

    return 1;
}

static int bind_bdev_get_block_count(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    lua_pushnumber(L, dev->block_count);

    return 1;
}

static int bind_bdev_read_bytes(lua_State *L) {
    require_exactly(L, 3);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    uint64_t start   = luaL_checknumber(L, 2);
    uint64_t len     = luaL_checknumber(L, 3);
    lua_pop(L, 3);

    uint8_t *data;
    if ( (data = malloc(len)) == NULL )
        err(1, "Couldn't allocate space for data read");

    if ( dev->read_bytes(dev, start, len, data) )
        lua_pushlstring(L, (char*)data, len);
    else
        lua_pushnil(L);

    free(data);

    return 1;
}

static int bind_bdev_write_bytes(lua_State *L) {
    require_exactly(L, 3);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev    = lua_touserdata(L, 1);
    uint64_t start      = luaL_checknumber(L, 2);
    size_t len;
    const uint8_t *data = (const uint8_t *) luaL_checklstring(L, 3, &len);

    bool ret = dev->write_bytes(dev, start, len, data);
    lua_pop(L, 3);

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_bdev_read_block(lua_State *L) {
    require_exactly(L, 2);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    uint64_t which   = luaL_checknumber(L, 2);
    lua_pop(L, 2);

    uint8_t *data;
    if ( (data = malloc(dev->block_size)) == NULL )
        err(1, "Couldn't allocate space for block read");

    if ( dev->read_block(dev, which, data) )
        lua_pushlstring(L, (char*)data, dev->block_size);
    else
        lua_pushnil(L);

    free(data);

    return 1;
}

static int bind_bdev_write_block(lua_State *L) {
    require_exactly(L, 3);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev    = lua_touserdata(L, 1);
    uint64_t which      = luaL_checknumber(L, 2);
    size_t len;
    const uint8_t *data = (const uint8_t *) luaL_checklstring(L, 3, &len);

    if ( len != dev->block_size ) {
        luaL_where(L, 1);
        lua_pushliteral(L, "Wrong block size (got ");
        lua_pushnumber(L, len);
        lua_pushliteral(L, " bytes, expected ");
        lua_pushnumber(L, dev->block_size);
        lua_pushliteral(L, "bytes)");
        lua_concat(L, 6);
        lua_error(L);
    }

    bool ret = dev->write_block(dev, which, data);
    lua_pop(L, 3);

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_bdev_close(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    dev->close(dev);

    return 0;
}

static int bind_bdev_clear_caches(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    dev->clear_caches(dev);

    return 0;
}

static int bind_bdev_flush(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    dev->flush(dev);

    return 0;
}

static int bind_bdev_sync(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    lua_pop(L, 1);

    dev->sync(dev);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/baseio.h"

static int bind_bio_create_malloc(lua_State *L) {
    require_exactly(L, 2);

    uint64_t block_size = luaL_checknumber(L, 1);
    size_t blocks       = luaL_checknumber(L, 2);
    lua_pop(L, 2);

    void *p = bio_create_malloc(block_size, blocks);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    return 1;
}

static int bind_bio_create_mmap(lua_State *L) {
    require_exactly(L, 4);

    uint64_t block_size = luaL_checknumber(L, 1);
    int fd              = luaL_checknumber(L, 2);
    size_t blocks       = luaL_checknumber(L, 3);
    off_t offset        = luaL_checknumber(L, 4);
    lua_pop(L, 4);

    void *p = bio_create_mmap(block_size, fd, blocks, offset);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    return 1;
}

static int bind_bio_create_posixfd(lua_State *L) {
    require_exactly(L, 4);

    uint64_t block_size = luaL_checknumber(L, 1);
    int fd              = luaL_checknumber(L, 2);
    size_t blocks       = luaL_checknumber(L, 3);
    off_t offset        = luaL_checknumber(L, 4);
    lua_pop(L, 4);

    void *p = bio_create_posixfd(block_size, fd, blocks, offset);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/concat.h"

static int bind_concat_open(lua_State *L) {
    require_atleast(L, 1);

    // TODO: allow argument to be a table of the devices
    
    int count = lua_gettop(L);

    struct bdev **devices;
    if ( (devices = malloc(sizeof(struct bdev *) * count)) == NULL )
        err(1, "Couldn't allocate space for devices list");

    for (int i = 0; i < count; i++) {
        if ( !lua_islightuserdata(L, i+1) ) {
            free(devices);
            return luaL_argerror(L, i+1, "not a light userdata");
        }
        devices[i] = lua_touserdata(L, i+1);
    }

    lua_pop(L, count);

    void *p = concat_open(devices, count);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    free(devices);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/encrypt.h"

static int bind_encrypt_create(lua_State *L) {
    require_exactly(L, 2);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    size_t keylen;
    const uint8_t *key = (const uint8_t *) luaL_checklstring(L, 2, &keylen);

    bool ret = encrypt_create(dev, key, keylen);

    lua_pop(L, 2); // here to make sure key is valid above

    lua_pushboolean(L, ret);

    return 1;
}

static int bind_encrypt_open(lua_State *L) {
    require_exactly(L, 2);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *dev = lua_touserdata(L, 1);
    size_t keylen;
    const uint8_t *key = (const uint8_t *) luaL_checklstring(L, 2, &keylen);

    struct bdev *ret = encrypt_open(dev, key, keylen);

    lua_pop(L, 2);
    
    if ( ret ) lua_pushlightuserdata(L, ret);
    else       lua_pushnil(L);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/slice.h"

static int bind_slice_open(lua_State *L) {
    require_exactly(L, 3);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *base = lua_touserdata(L, 1);
    uint64_t start    = luaL_checknumber(L, 2);
    uint64_t len      = luaL_checknumber(L, 3);
    lua_pop(L, 3);

    struct bdev *dev = slice_open(base, start, len);

    if ( dev ) lua_pushlightuserdata(L, dev);
    else       lua_pushnil(L);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/stripe.h"

static int bind_stripe_open(lua_State *L) {
    require_atleast(L, 1);

    // TODO: allow argument to be a table of the devices
    
    int count = lua_gettop(L);

    struct bdev **devices;
    if ( (devices = malloc(sizeof(struct bdev *) * count)) == NULL )
        err(1, "Couldn't allocate space for devices list");

    for (int i = 0; i < count; i++) {
        if ( !lua_islightuserdata(L, i+1) ) {
            free(devices);
            return luaL_argerror(L, i+1, "not a light userdata");
        }
        devices[i] = lua_touserdata(L, i+1);
    }

    lua_pop(L, count);

    void *p = stripe_open(devices, count);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    free(devices);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/verify.h"

static int bind_verify_create(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *base = lua_touserdata(L, 1);
    lua_pop(L, 1);

    struct bdev *ret = verify_create(base);
    if ( ret ) lua_pushlightuserdata(L, ret);
    else       lua_pushnil(L);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
#include "layers/lazyzero.h"

static int bind_lazyzero_create(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *base = lua_touserdata(L, 1);
    lua_pop(L, 1);

    lua_pushboolean(L, lazyzero_create(base));

    return 1;
}

static int bind_lazyzero_open(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *base = lua_touserdata(L, 1);
    lua_pop(L, 1);

    struct bdev *ret = lazyzero_open(base);
    if ( ret ) lua_pushlightuserdata(L, ret);
    else       lua_pushnil(L);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// finalizer for bdevs

static int bind_close_on_gc_finalizer(lua_State *L) {
    struct bdev **ptr = luaL_checkudata(L, 1, "druid closeongc");
    lua_pop(L, 1);
    (*ptr)->close(*ptr);
    return 0;
}

static int bind_close_on_gc(lua_State *L) {
    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct bdev *base = lua_touserdata(L, 1);
    lua_pop(L, 1);

    struct bdev **ptr = lua_newuserdata(L, sizeof(struct bdev *));
    *ptr = base;
    int at = lua_gettop(L);

    if ( luaL_newmetatable(L, "druid closeongc") ) {
        int table = lua_gettop(L);
        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, bind_close_on_gc_finalizer);
        lua_settable(L, table);
    }

    lua_setmetatable(L, at);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

int bind_bdevs(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "bdev_get_block_size", bind_bdev_get_block_size },
        { "bdev_get_block_count", bind_bdev_get_block_count },
        { "bdev_read_bytes", bind_bdev_read_bytes },
        { "bdev_write_bytes", bind_bdev_write_bytes },
        { "bdev_read_block", bind_bdev_read_block },
        { "bdev_write_block", bind_bdev_write_block },
        { "bdev_close", bind_bdev_close },
        { "bdev_clear_caches", bind_bdev_clear_caches },
        { "bdev_flush", bind_bdev_flush },
        { "bdev_sync", bind_bdev_sync },

        { "bio_create_malloc", bind_bio_create_malloc },
        { "bio_create_mmap", bind_bio_create_mmap },
        { "bio_create_posixfd", bind_bio_create_posixfd },

        { "concat_open", bind_concat_open },

        { "encrypt_create", bind_encrypt_create },
        { "encrypt_open", bind_encrypt_open },

        { "slice_open", bind_slice_open },

        { "stripe_open", bind_stripe_open },

        { "verify_create", bind_verify_create },

        { "lazyzero_create", bind_lazyzero_create },
        { "lazyzero_open", bind_lazyzero_open },

        { "close_on_gc", bind_close_on_gc },

        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

