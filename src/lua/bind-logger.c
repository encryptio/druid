#include "bind.h"

#include "logger.h"

static const char *level_names[] = { "junk", "info", "warn", "err", "all", "none", "unknown", NULL };
static const int level_correspondence[] = { LOG_JUNK, LOG_INFO, LOG_WARN, LOG_ERR, LOG_ALL, LOG_NONE, -1 };

static int bind_log_fn(lua_State *L) {
    require_exactly(L, 3);

    int level = level_correspondence[ luaL_checkoption(L, 1, "unknown", level_names) ];
    const char *module = luaL_checkstring(L, 2);
    const char *str    = luaL_checkstring(L, 3);

    logger(level, module, "%s", str);

    return 0;
}

// TODO: bind logger_set_output sanely

static int bind_log_set_level(lua_State *L) {
    require_exactly(L, 1);

    int level = level_correspondence[ luaL_checkoption(L, 1, NULL, level_names) ];

    if ( level == -1 ) {
        // someone actually *chose* "unknown"
        luaL_where(L, 1);
        lua_pushliteral(L, "Unknown logging level");
        lua_concat(L, 2);
        lua_error(L);
    }

    logger_set_level(level);

    return 0;
}

int bind_logger(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "log", bind_log_fn },
        { "log_set_level", bind_log_set_level },
        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

