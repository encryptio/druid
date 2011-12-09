#include "lua/raw-bindings.h"

#include <stdlib.h>
#include <err.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static inline void require_atleast(lua_State *L, int ct) {
    int got = lua_gettop(L);
    if ( got < ct ) {
        luaL_where(L, 1);
        lua_pushliteral(L, "Need more arguments (wanted ");
        lua_pushnumber(L, ct);
        lua_pushliteral(L, ", got ");
        lua_pushnumber(L, got);
        lua_pushliteral(L, ")");
        lua_concat(L, 6);
        lua_error(L);
    }
}

static inline void require_exactly(lua_State *L, int ct) {
    int got = lua_gettop(L);
    if ( got != ct ) {
        luaL_where(L, 1);
        lua_pushliteral(L, "Wrong number of arguments (wanted ");
        lua_pushnumber(L, ct);
        lua_pushliteral(L, ", got ");
        lua_pushnumber(L, got);
        lua_pushliteral(L, ")");
        lua_concat(L, 6);
        lua_error(L);
    }
}

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
#include "layers/nbd.h"

static int bind_nbd_create(lua_State *L) {
    require_exactly(L, 2);

    if ( !lua_islightuserdata(L, 2) )
        return luaL_argerror(L, 2, "not a light userdata");

    int port = luaL_checkint(L, 1);
    struct bdev *dev = lua_touserdata(L, 2);
    lua_pop(L, 2);

    if ( port < 2 || port > 65535 )
        return luaL_argerror(L, 1, "port is out of range");

    struct nbd_server *srv = nbd_create(port, dev);

    if ( srv ) lua_pushlightuserdata(L, srv);
    else       lua_pushnil(L);

    return 1;
}

static int bind_nbd_listenloop(lua_State *L) {
    require_exactly(L, 1);

    if ( !lua_islightuserdata(L, 1) )
        return luaL_argerror(L, 1, "not a light userdata");

    struct nbd_server *srv = lua_touserdata(L, 1);
    lua_pop(L, 1);

    nbd_listenloop(srv);

    // never reaches here, but whatever

    return 0;
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
// public interface

void bind_druidraw(lua_State *L) {
    lua_newtable(L); // will be druidraw in global table
    int table = lua_gettop(L);

#define BIND(name) do { \
    lua_pushliteral(L, #name); \
    lua_pushcfunction(L, bind_##name); \
    lua_settable(L, table); \
} while (0)

    BIND(bdev_get_block_size);
    BIND(bdev_get_block_count);
    BIND(bdev_read_bytes);
    BIND(bdev_write_bytes);
    BIND(bdev_read_block);
    BIND(bdev_write_block);
    BIND(bdev_close);
    BIND(bdev_clear_caches);
    BIND(bdev_flush);
    BIND(bdev_sync);

    BIND(bio_create_malloc);
    BIND(bio_create_mmap);
    BIND(bio_create_posixfd);

    BIND(concat_open);

    BIND(encrypt_create);
    BIND(encrypt_open);

    BIND(nbd_create);
    BIND(nbd_listenloop);

    BIND(slice_open);

    BIND(stripe_open);

    BIND(verify_create);

#undef BIND

    lua_setglobal(L, "druidraw");
}
