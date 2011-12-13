#include "lua/raw-bindings.h"

#include <stdlib.h>
#include <err.h>
#include <assert.h>
#include <string.h>

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
#include "logger.h"

static int bind_logger(lua_State *L) {
    require_exactly(L, 3);

    int level = luaL_checkint(L, 1);
    const char *module = luaL_checkstring(L, 2);
    const char *str    = luaL_checkstring(L, 3);

    logger(level, module, "%s", str);

    return 0;
}

static int bind_logger_set_output(lua_State *L) {
    require_exactly(L, 1);

    int fd = luaL_checkint(L, 1);

    logger_set_output(fd);

    return 0;
}

static int bind_logger_set_level(lua_State *L) {
    require_exactly(L, 1);

    int level = luaL_checkint(L, 1);

    logger_set_level(level);

    return 0;
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
#include "loop.h"

struct timer_data {
    lua_State *L;
    int refnum;
};

static void bind_loop_timer_cb(void *data) {
    struct timer_data *t = data;
    lua_State *L = t->L;
    int refnum = t->refnum;
    free(t);

    lua_rawgeti(L, LUA_REGISTRYINDEX, refnum);
    if ( lua_pcall(L, 0, 0, 0) ) {
        loop_exit_early();
        luaL_unref(L, LUA_REGISTRYINDEX, refnum);
        lua_error(L); // TODO: is it possible for Lua to catch this?
    }
}

static int bind_loop_add_timer(lua_State *L) {
    require_exactly(L, 2);

    double in = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct timer_data *t;
    if ( (t = malloc(sizeof(struct timer_data))) == NULL )
        err(1, "Couldn't allocate space for timer_data");

    t->refnum = luaL_ref(L, LUA_REGISTRYINDEX); // TODO: is the registry okay to do this with?
    t->L = L;

    loop_add_timer(in, bind_loop_timer_cb, t);

    lua_pop(L, 1);

    return 0;
}

static int bind_loop_exit_early(lua_State *L) {
    require_exactly(L, 0);

    loop_exit_early();

    return 0;
}

enum socket_state {
    SOCKET_CONNECTING,
    SOCKET_OPEN,
    SOCKET_DESTROYED,
};

////////////////////////////////////////
// Socket functions

#define SOCKET_READ_SIZE 1024
#define SOCKET_HOST_LEN 64

struct socket_data {
    lua_State *L;

    enum socket_state state;

    int error_cb_ref;
    int connect_cb_ref;
    int read_cb_ref;

    int sd_ref;

    struct loop_sockhandle *sock;

    uint8_t buffer[SOCKET_READ_SIZE];

    char host[SOCKET_HOST_LEN];
    uint16_t port;
};

static int bind_loop_sock_gc(lua_State *L) {
    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    luaL_unref(L, LUA_REGISTRYINDEX, sd->sd_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->error_cb_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->connect_cb_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->read_cb_ref);

    loop_sock_close(sd->sock);
    sd->sock = NULL;

    sd->state = SOCKET_DESTROYED;

    lua_pop(L, 1);

    return 0;
}

static int bind_loop_sock_write(lua_State *L) {
    require_exactly(L, 2);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    size_t amt = 0;
    const char *buf = luaL_checklstring(L, 2, &amt);
    assert(buf != NULL);

    loop_sock_write(sd->sock, (const uint8_t *)buf, amt);

    lua_pop(L, 2);

    return 0;
}

static int bind_loop_sock_close(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    return bind_loop_sock_gc(L);
}

static int bind_loop_sock_get_hostname(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushstring(L, sd->host);
    
    return 1;
}

static int bind_loop_sock_get_port(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushinteger(L, sd->port);

    return 1;
}

static void bind_loop_read_cb(size_t in_buffer, struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED ) {
        logger(LOG_ERR, "bind", "Read callback called on an already destroyed socket");
        return;
    }

    while ( in_buffer > 0 ) {
        size_t ret = loop_sock_peek(h, sd->buffer, SOCKET_READ_SIZE);
        assert(ret > 0);

        loop_sock_drop(h, ret);
        in_buffer -= ret;

        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->read_cb_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
        lua_pushlstring(L, (char *)sd->buffer, ret);
        lua_call(L, 2, 0);
    }
}

static void bind_loop_connect_cb(struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED ) {
        logger(LOG_ERR, "bind", "Connect callback called on an already destroyed socket");
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->connect_cb_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_call(L, 1, 0);
}

static void bind_loop_error_cb(int err, struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED )
        return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->error_cb_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_pushinteger(L, err);
    lua_call(L, 2, 0);

    lua_pushcfunction(L, bind_loop_sock_gc);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_call(L, 1, 0);
}

static int bind_loop_tcp_connect(lua_State *L) {
    require_exactly(L, 5);

    const char *host = luaL_checkstring(L, 1);
    int port         = luaL_checkint(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION); // TODO: allow this to be nil for "raise errors"
    luaL_checktype(L, 4, LUA_TFUNCTION);
    luaL_checktype(L, 5, LUA_TFUNCTION);

    if ( port < 0 || port > 65535 ) {
        lua_pushliteral(L, "Port ");
        lua_pushinteger(L, port);
        lua_pushliteral(L, " is out of range [0..65535]");
        lua_concat(L, 3);
        lua_error(L);
    }

    struct socket_data *sd = lua_newuserdata(L, sizeof(struct socket_data));
    assert(sd);
    sd->sd_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    sd->L = L;
    sd->sock = loop_tcp_connect(host, port, bind_loop_error_cb, bind_loop_connect_cb, bind_loop_read_cb, sd);

    if ( !sd->sock ) {
        luaL_unref(L, LUA_REGISTRYINDEX, sd->sd_ref);

        luaL_where(L, 1);
        lua_pushliteral(L, "Couldn't setup TCP connection");
        lua_concat(L, 2);
        lua_error(L);
    }

    sd->read_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    sd->connect_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    sd->error_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    sd->state = SOCKET_CONNECTING;
    
    strncpy(sd->host, host, SOCKET_HOST_LEN);
    sd->host[SOCKET_HOST_LEN-1] = '\0';
    
    sd->port = port;

    lua_pop(L, 2); // host,port

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);

    // now the userdata is the only thing on the stack, no extraneous references
    assert(lua_gettop(L) == 1);

    if ( luaL_newmetatable(L, "druid socket") ) {
        int table = lua_gettop(L);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, bind_loop_sock_gc);
        lua_settable(L, table);

        lua_pushliteral(L, "__index");
        if ( luaL_newmetatable(L, "druid socket methods") ) {
            int table = lua_gettop(L);

            lua_pushliteral(L, "write");
            lua_pushcfunction(L, bind_loop_sock_write);
            lua_settable(L, table);

            lua_pushliteral(L, "close");
            lua_pushcfunction(L, bind_loop_sock_close);
            lua_settable(L, table);

            lua_pushliteral(L, "get_hostname");
            lua_pushcfunction(L, bind_loop_sock_get_hostname);
            lua_settable(L, table);

            lua_pushliteral(L, "get_port");
            lua_pushcfunction(L, bind_loop_sock_get_port);
            lua_settable(L, table);
        }
        lua_settable(L, table);
    }

    lua_setmetatable(L, 1);

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

    BIND(lazyzero_create);
    BIND(lazyzero_open);

    BIND(logger);
    BIND(logger_set_output);
    BIND(logger_set_level);

    BIND(close_on_gc);

    BIND(loop_add_timer);
    BIND(loop_exit_early);

    BIND(loop_tcp_connect);

#undef BIND

    lua_setglobal(L, "druidraw");
}
