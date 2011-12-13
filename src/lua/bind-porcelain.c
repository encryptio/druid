#include "lua/bind.h"

#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "lua/AUTOGEN-porcelain-data.h"

static const char *reader(lua_State *L, void *data, size_t *size) {
    bool *loaded = data;
    if ( *loaded ) return NULL;
    *loaded = true;
    *size = strlen((char*) druid_lua_porcelain);
    return (char*) druid_lua_porcelain;
}

int bind_porcelain(lua_State *L) {
    require_exactly(L, 0);

    bool loaded = false;
    if ( lua_load(L, reader, &loaded, "druid.lua") ) {
        lua_pushliteral(L, "Error while compiling druid.lua: ");
        lua_pushvalue(L, 1);
        lua_concat(L, 2);
        lua_error(L);
    }

    if ( lua_pcall(L, 0, 0, 0) ) {
        lua_pushliteral(L, "Error while executing druid.lua: ");
        lua_pushvalue(L, 1);
        lua_concat(L, 2);
        lua_error(L);
    }

    return 0;
}

