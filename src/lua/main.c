#include "lua/bind.h"
#include "loop.h"
#include "logger.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>

#include <readline/readline.h>
#include <readline/history.h>

////////////////////////////////////////////////////////////////////////////////
// GC loop

// TODO: tune for common usage
#define GC_INTERVAL 10

static void gc_loop_fn(void *data) {
    static int gc_counter = 0;
    gc_counter++;

    if ( gc_counter == GC_INTERVAL ) {
        gc_counter = 0;
        lua_State *L = data;
        lua_gc(L, LUA_GCSTEP, 1);
    }
}

////////////////////////////////////////////////////////////////////////////////

static bool run_interactive = false;
static bool interactive_loop_running = false;

static void end_interactive_loop(void);

// pfft, non-reentrant readline crap
lua_State *lua_state_used_for_readline = NULL;
static void got_line(char *line) {
    if ( line == NULL ) {
        // EOF
        end_interactive_loop();
        return;
    }

    lua_State *L = lua_state_used_for_readline;
    assert(L != NULL);

    if ( strlen(line) )
        add_history(line);

    if ( luaL_dostring(L, line) ) {
        fprintf(stderr, "%s\n", luaL_checkstring(L, -1));
        lua_pop(L, 1);
    }
}

static void stdin_callback_fn(void *data) {
    rl_callback_read_char();
}

static struct loop_watcher *watcher = NULL;
static void start_interactive_loop(lua_State *L) {
    assert(!interactive_loop_running);

    // TODO: should open /dev/tty?
    if ( !isatty(fileno(stdout)) || !isatty(fileno(stdin)) )
        errx(1, "Can't open an interactive shell when stdin or stdout are not a tty");

    lua_state_used_for_readline = L;
    rl_callback_handler_install("druid> ", got_line);
    watcher = loop_watch_fd_for_reading(0, stdin_callback_fn, NULL);
    assert(watcher);
    interactive_loop_running = true;
}

static void end_interactive_loop(void) {
    if ( interactive_loop_running ) {
        rl_callback_handler_remove();
        loop_stop_watching(watcher);
        watcher = NULL;

        // go to the beginning of the line and clear it
        fprintf(stdout, "\r\033[K");

        interactive_loop_running = false;
    }
}

////////////////////////////////////////////////////////////////////////////////

static int runloop(lua_State *L) {
    lua_pop(L, 1); // pop the userdata from lua_cpcall

    loop_until_done();

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

    loop_do_repeatedly_whenever(gc_loop_fn, L);

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
        start_interactive_loop(L);

    if ( lua_cpcall(L, runloop, NULL) ) {
        logger(LOG_ERR, "main", "Uncaught Lua error after main loop started: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    if ( run_interactive )
        end_interactive_loop();

    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_close(L);

    loop_teardown();

    exit(0);
}

