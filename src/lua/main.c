#include "lua/raw-bindings.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <readline/readline.h>
#include <readline/history.h>

static void interactive_loop(lua_State *L) {
    char *s;
    while ( (s = readline("druid> ")) != NULL ) {
        add_history(s);

        if ( luaL_dostring(L, s) ) {
            fprintf(stderr, "%s\n", luaL_checkstring(L, -1));
            lua_pop(L, 1);
        }

        lua_gc(L, LUA_GCCOLLECT, 0);
    }
    
    // go to the beginning of the line and clear it
    fprintf(stdout, "\r\033[K");
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    assert(L);

    luaL_openlibs(L);
    bind_druidraw(L);

    lua_gc(L, LUA_GCCOLLECT, 0);

    if ( argc >= 2 ) {
        if ( luaL_dofile(L, argv[1]) ) {
            fprintf(stderr, "error executing '%s': %s\n", argv[1], luaL_checkstring(L, -1));
            exit(1);
        }
    }

    lua_gc(L, LUA_GCCOLLECT, 0);

    if ( isatty(fileno(stdout)) )
        interactive_loop(L);

    exit(0);
}

