#ifndef __LUA_BIND_H__
#define __LUA_BIND_H__

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int bind_socket(lua_State *L); // bind-socket.c 
int bind_timer(lua_State *L);  // bind-timer.c 
int bind_logger(lua_State *L); // bind-logger.c
int bind_bdevs(lua_State *L);  // bind-bdevs.c

int bind_druidraw(lua_State *L); // bind.c

// helpers for the bindings themselves
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

#endif
