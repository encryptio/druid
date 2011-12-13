#include "bind.h"

#include <stdlib.h>
#include <err.h>

#include "loop.h"

struct timer_data {
    lua_State *L;
    int refnum;
};

static void timer_cb(void *data) {
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

static int bind_a_timer(lua_State *L) {
    require_exactly(L, 2);

    double in = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct timer_data *t;
    if ( (t = malloc(sizeof(struct timer_data))) == NULL )
        err(1, "Couldn't allocate space for timer_data");

    t->refnum = luaL_ref(L, LUA_REGISTRYINDEX); // TODO: is the registry okay to do this with?
    t->L = L;

    loop_add_timer(in, timer_cb, t);

    lua_pop(L, 1);

    return 0;
}

static int bind_stop_loop(lua_State *L) {
    require_exactly(L, 0);

    loop_exit_early();

    return 0;
}

int bind_timer(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "timer", bind_a_timer },
        { "stop_loop", bind_stop_loop },
        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

