#include "bind.h"

// all the actual function binding happens in bind-*.c.
// this is just a wrapper to do them all and save it into
// the global table druidraw.

int bind_druidraw(lua_State *L) {
    require_exactly(L, 0);

    lua_newtable(L);

    lua_CFunction all[] = {
        bind_socket,
        bind_timer,
        bind_logger,
        bind_bdevs,
        NULL
    };

    for (int i = 0; all[i]; i++) {
        lua_pushcfunction(L, all[i]);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 0);
    }

    lua_setglobal(L, "druid");

    return 0;
}

