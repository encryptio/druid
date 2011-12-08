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
        lua_pushstring(L, "Need more arguments (wanted ");
        lua_pushnumber(L, ct);
        lua_pushstring(L, ", got ");
        lua_pushnumber(L, got);
        lua_pushstring(L, ")");
        lua_concat(L, 6);
        lua_error(L);
    }
}

static inline void require_exactly(lua_State *L, int ct) {
    int got = lua_gettop(L);
    if ( got != ct ) {
        luaL_where(L, 1);
        lua_pushstring(L, "Wrong number of arguments (wanted ");
        lua_pushnumber(L, ct);
        lua_pushstring(L, ", got ");
        lua_pushnumber(L, got);
        lua_pushstring(L, ")");
        lua_concat(L, 6);
        lua_error(L);
    }
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

    void *p = concat_open(devices, count);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    free(devices);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// public interface

void bind_druidraw(lua_State *L) {
    lua_newtable(L); // will be druidraw in global table
    int table = lua_gettop(L);

#define BIND(name) do { \
    lua_pushstring(L, #name); \
    lua_pushcfunction(L, bind_##name); \
    lua_settable(L, table); \
} while (0)

    BIND(bio_create_malloc);
    BIND(bio_create_mmap);
    BIND(bio_create_posixfd);
    BIND(concat_open);

#undef BIND

    lua_setglobal(L, "druidraw");
}
