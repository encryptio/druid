#include "lua/porcelain.h"

#include "lua/AUTOGEN-porcelain-data.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static const char *reader(lua_State *L, void *data, size_t *size) {
    bool *loaded = data;
    if ( *loaded ) return NULL;
    *loaded = true;
    *size = strlen((char*) druid_lua_porcelain);
    return (char*) druid_lua_porcelain;
}

void bind_druid_porcelain(lua_State *L) {
    bool loaded = false;
    if ( lua_load(L, reader, &loaded, "druid.lua") ) {
        fprintf(stderr, "Error while compiling druid.lua: %s\n", luaL_checkstring(L, -1));
        exit(1);
    }
    if ( lua_pcall(L, 0, 0, 0) ) {
        fprintf(stderr, "Error while executing druid.lua: %s\n", luaL_checkstring(L, -1));
        exit(1);
    }
}

