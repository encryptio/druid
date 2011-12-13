#include "bind.h"

#include "logger.h"

static int bind_logger_fn(lua_State *L) {
    require_exactly(L, 3);

    int level = luaL_checkint(L, 1);
    const char *module = luaL_checkstring(L, 2);
    const char *str    = luaL_checkstring(L, 3);

    logger(level, module, "%s", str);

    return 0;
}

static int bind_logger_set_output(lua_State *L) {
    require_exactly(L, 1);

    int fd = luaL_checkint(L, 1);

    logger_set_output(fd);

    return 0;
}

static int bind_logger_set_level(lua_State *L) {
    require_exactly(L, 1);

    int level = luaL_checkint(L, 1);

    logger_set_level(level);

    return 0;
}

int bind_logger(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "logger", bind_logger_fn },
        { "logger_set_output", bind_logger_set_output },
        { "logger_set_level", bind_logger_set_level },
        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

