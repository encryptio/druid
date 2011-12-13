#include "lua/bind.h"
#include "loop.h"
#include "logger.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <readline/readline.h>
#include <readline/history.h>

static bool run_interactive = false;

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

static int runloop(lua_State *L) {
    lua_pop(L, 1); // pop the userdata from lua_cpcall

    loop_until_done();

    // TODO: make this work well with the event loop
    if ( run_interactive )
        interactive_loop(L);

    return 0;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    assert(L);

    luaL_openlibs(L);

    lua_pushcfunction(L, bind_druidraw);
    lua_call(L, 0, 0);

    lua_pushcfunction(L, bind_porcelain);
    lua_call(L, 0, 0);

    lua_gc(L, LUA_GCCOLLECT, 0);

    if ( lua_gettop(L) )
        lua_pop(L, lua_gettop(L));

    loop_setup();

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

    if ( lua_cpcall(L, runloop, NULL) ) {
        logger(LOG_ERR, "main", "Uncaught Lua error after main loop started: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_close(L);

    loop_teardown();

    exit(0);
}

