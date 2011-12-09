#include "lua/raw-bindings.h"
#include "lua/porcelain.h"

#include <stdbool.h>
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
    if ( !isatty(fileno(stdout)) ) {
        fprintf(stderr, "Can't open an interactive shell when stdout is not a tty");
        exit(1);
    }

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
    bind_druid_porcelain(L);

    lua_gc(L, LUA_GCCOLLECT, 0);

    if ( lua_gettop(L) )
        lua_pop(L, lua_gettop(L));

    bool run_interactive = false;

    for (int i = 1; i < argc; i++) {
        if ( strcmp(argv[i], "-i") == 0 ) {
            run_interactive = true;
        } else if ( luaL_dofile(L, argv[i]) ) {
            fprintf(stderr, "error executing '%s': %s\n", argv[i], luaL_checkstring(L, -1));
            exit(1);
        }

        if ( lua_gettop(L) )
            lua_pop(L, lua_gettop(L));

        lua_gc(L, LUA_GCCOLLECT, 0);
    }

    if ( argc == 1 )
        run_interactive = true;

    if ( run_interactive )
        interactive_loop(L);

    lua_gc(L, LUA_GCCOLLECT, 0);

    exit(0);
}

