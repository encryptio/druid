#include "lua/raw-bindings.h"

#include "layers/baseio.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

////////////////////////////////////////////////////////////////////////////////
// for layers/baseio.h

static int bind_bio_create_malloc(lua_State *L) {
    if ( lua_gettop(L) != 2 ) {
        lua_pushstring(L, "Incorrect number of arguments");
        lua_error(L);
    }

    uint64_t block_size = luaL_checknumber(L, 1);
    size_t blocks       = luaL_checknumber(L, 2);
    lua_pop(L, 2);

    void *p = bio_create_malloc(block_size, blocks);
    if ( p ) lua_pushlightuserdata(L, p);
    else     lua_pushnil(L);

    return 1;
}

static int bind_bio_create_mmap(lua_State *L) {
    if ( lua_gettop(L) != 4 ) {
        lua_pushstring(L, "Incorrect number of arguments");
        lua_error(L);
    }

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
    if ( lua_gettop(L) != 4 ) {
        lua_pushstring(L, "Incorrect number of arguments");
        lua_error(L);
    }

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

#undef BIND

    lua_setglobal(L, "druidraw");
}

