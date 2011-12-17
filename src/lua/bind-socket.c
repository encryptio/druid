#include "lua/bind.h"

#include <stdbool.h>
#include <stdlib.h>
#include <err.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include "loop.h"
#include "logger.h"

enum socket_state {
    SOCKET_CONNECTING,
    SOCKET_OPEN,
    SOCKET_DESTROYED,
};

#define SOCKET_READ_SIZE 1024
#define SOCKET_HOST_LEN 64

struct socket_cbrefs {
    int error;
    int connect;
    int eof;
    int read;
};

struct socket_data {
    lua_State *L;

    enum socket_state state;

    struct socket_cbrefs cb;
    int sd_ref;

    struct loop_sockhandle *sock;

    uint8_t buffer[SOCKET_READ_SIZE];

    char host[SOCKET_HOST_LEN];
    uint16_t port;
};

////////////////////////////////////////////////////////////////////////////////

// building block
// input stack: |BOTTOM|> socket, table/fn, <TOP>
// the top of the stack is the table or function of callbacks, and the socket is
// the userdata containing a partially initialized socket.
// 
// output stack: |BOTTOM> socket <TOP>
// output stack: |BOTTOM> socket, errmsg <TOP> - when return value is false
//
// returns false failure, true on success
static bool bind_loop_sock_setup_cbrefs(lua_State *L) {
    struct socket_data *sd = lua_touserdata(L, -2);
    assert(sd != NULL);

    sd->cb.read    = LUA_REFNIL;
    sd->cb.eof     = LUA_REFNIL;
    sd->cb.error   = LUA_REFNIL;
    sd->cb.connect = LUA_REFNIL;

    if ( lua_type(L, -1) == LUA_TFUNCTION ) {
        lua_pushvalue(L, -2);
        if ( lua_pcall(L, 1, 1, 0) )
            return false;

        if ( lua_type(L, -1) != LUA_TTABLE ) {
            lua_pop(L, 1);
            lua_pushliteral(L, "Callback did not return a table");
            return false;
        }

    } else if ( lua_type(L, -1) != LUA_TTABLE ) {
        lua_pop(L, 1);
        lua_pushliteral(L, "Callback object is not a function or a table");
        return false;
    }

    lua_getfield(L, -1, "read");
    if ( lua_type(L, -1) != LUA_TFUNCTION ) {
        const char *typename = lua_typename(L, lua_type(L, -1));
        lua_pop(L, 2);

        lua_pushliteral(L, "'read' field of callback table is not a function (is ");
        lua_pushstring(L, typename);
        lua_pushliteral(L, ")");
        lua_concat(L, 3);

        return false;
    }
    sd->cb.read = luaL_ref(L, LUA_REGISTRYINDEX);

    // TODO: add default implementation, which does nothing
    lua_getfield(L, -1, "eof");
    if ( lua_type(L, -1) != LUA_TFUNCTION ) {
        const char *typename = lua_typename(L, lua_type(L, -1));
        lua_pop(L, 2);

        lua_pushliteral(L, "'eof' field of callback table is not a function (is ");
        lua_pushstring(L, typename);
        lua_pushliteral(L, ")");
        lua_concat(L, 3);

        return false;
    }
    sd->cb.eof = luaL_ref(L, LUA_REGISTRYINDEX);

    // TODO: add default implementation, which does nothing
    lua_getfield(L, -1, "connect");
    if ( lua_type(L, -1) != LUA_TFUNCTION ) {
        const char *typename = lua_typename(L, lua_type(L, -1));
        lua_pop(L, 2);

        lua_pushliteral(L, "'connect' field of callback table is not a function (is ");
        lua_pushstring(L, typename);
        lua_pushliteral(L, ")");
        lua_concat(L, 3);

        return false;
    }
    sd->cb.connect = luaL_ref(L, LUA_REGISTRYINDEX);

    // TODO: add default implementation, which raises errors sent to it
    lua_getfield(L, -1, "error");
    if ( lua_type(L, -1) != LUA_TFUNCTION ) {
        const char *typename = lua_typename(L, lua_type(L, -1));
        lua_pop(L, 2);

        lua_pushliteral(L, "'error' field of callback table is not a function (is ");
        lua_pushstring(L, typename);
        lua_pushliteral(L, ")");
        lua_concat(L, 3);

        return false;
    }
    sd->cb.error = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_pop(L, 1);

    return true;
}

////////////////////////////////////////////////////////////////////////////////

static int bind_loop_sock_gc(lua_State *L) {
    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    luaL_unref(L, LUA_REGISTRYINDEX, sd->sd_ref);

    luaL_unref(L, LUA_REGISTRYINDEX, sd->cb.error);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->cb.eof);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->cb.connect);
    luaL_unref(L, LUA_REGISTRYINDEX, sd->cb.read);

    loop_sock_close(sd->sock);
    sd->sock = NULL;

    sd->state = SOCKET_DESTROYED;

    lua_pop(L, 1);

    return 0;
}

static int bind_loop_sock_write(lua_State *L) {
    require_exactly(L, 2);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    size_t amt = 0;
    const char *buf = luaL_checklstring(L, 2, &amt);
    assert(buf != NULL);

    loop_sock_write(sd->sock, (const uint8_t *)buf, amt);

    lua_pop(L, 2);

    return 0;
}

static int bind_loop_sock_close(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    if ( sd->state == SOCKET_DESTROYED )
        return 0;

    return bind_loop_sock_gc(L);
}

static int bind_loop_sock_get_hostname(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushstring(L, sd->host);
    
    return 1;
}

static int bind_loop_sock_get_port(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushinteger(L, sd->port);

    return 1;
}

static int bind_loop_sock_id(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushnumber(L, (uintptr_t)sd );

    return 1;
}

static int bind_loop_sock_tostring(lua_State *L) {
    require_exactly(L, 1);

    struct socket_data *sd = luaL_checkudata(L, 1, "druid socket");
    lua_pop(L, 1);

    lua_pushliteral(L, "tcp(");

    if ( sd->state == SOCKET_CONNECTING )
        lua_pushliteral(L, "connecting to ");
    else if ( sd->state == SOCKET_OPEN )
        lua_pushliteral(L, "connected to ");
    else if ( sd->state == SOCKET_DESTROYED )
        lua_pushliteral(L, "finished with ");
    else
        errx(1, "bind_loop_sock_tostring had no valid sd->state");

    lua_pushstring(L, sd->host);
    lua_pushliteral(L, " port ");
    lua_pushinteger(L, sd->port);

    lua_pushliteral(L, ", id ");
    lua_pushnumber(L, (uintptr_t)sd );

    lua_pushliteral(L, ")");

    lua_concat(L, 8);
    
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

static void bind_loop_read_cb(size_t in_buffer, struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED ) {
        logger(LOG_ERR, "bind", "Read callback called on an already destroyed socket");
        return;
    }

    while ( in_buffer > 0 ) {
        size_t ret = loop_sock_peek(h, sd->buffer, SOCKET_READ_SIZE);
        assert(ret > 0);

        loop_sock_drop(h, ret);
        in_buffer -= ret;

        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->cb.read);
        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
        lua_pushlstring(L, (char *)sd->buffer, ret);
        lua_call(L, 2, 0);
    }
}

static void bind_loop_connect_cb(struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED ) {
        logger(LOG_ERR, "bind", "Connect callback called on an already destroyed socket");
        return;
    }

    sd->state = SOCKET_OPEN;

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->cb.connect);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_call(L, 1, 0);
}

static void bind_loop_error_cb(int err, struct loop_sockhandle *h, void *data) {
    struct socket_data *sd = data;
    lua_State *L = sd->L;

    if ( sd->state == SOCKET_DESTROYED )
        return;

    if ( err == 0 ) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->cb.eof);
    } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, sd->cb.error);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_pushinteger(L, err);
    lua_call(L, 2, 0);

    lua_pushcfunction(L, bind_loop_sock_gc);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);
    lua_call(L, 1, 0);
}

////////////////////////////////////////////////////////////////////////////////

static void bind_loop_socket_setmetatable(lua_State *L) {
    if ( luaL_newmetatable(L, "druid socket") ) {
        int table = lua_gettop(L);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, bind_loop_sock_gc);
        lua_settable(L, table);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, bind_loop_sock_tostring);
        lua_settable(L, table);

        lua_pushliteral(L, "__index");
        if ( luaL_newmetatable(L, "druid socket methods") ) {
            luaL_Reg fns[] = {
                { "write", bind_loop_sock_write },
                { "close", bind_loop_sock_close },
                { "get_hostname", bind_loop_sock_get_hostname },
                { "get_port", bind_loop_sock_get_port },
                { "tostring", bind_loop_sock_tostring },
                { "id", bind_loop_sock_id },
                { NULL, NULL }
            };

            luaL_register(L, NULL, fns);
        }
        lua_settable(L, table);
    }

    lua_setmetatable(L, 1);
}

static int bind_loop_tcp_connect(lua_State *L) {
    require_exactly(L, 3);

    const char *host = luaL_checkstring(L, 1);
    int port         = luaL_checkint(L, 2);
    luaL_argcheck(L, lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TTABLE, 3,
            "Handler is not a function or table");

    luaL_argcheck(L, port >= 0 && port <= 65535, 2, "Port is out of range");
    luaL_argcheck(L, strlen(host) > 0, 1, "Hostname is empty");

    struct socket_data *sd = lua_newuserdata(L, sizeof(struct socket_data));
    assert(sd);
    lua_pushvalue(L, -1);
    sd->sd_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    bind_loop_socket_setmetatable(L);

    // stack: host, port, handler, sd
    assert(lua_gettop(L) == 4);

    lua_pushvalue(L, 3); // stack: ..., sd, handler
    if ( !bind_loop_sock_setup_cbrefs(L) ) {
        // stack: host, port, handler, sd, errmsg
        assert(lua_gettop(L) == 5);

        luaL_unref(L, LUA_REGISTRYINDEX, sd->sd_ref);

        luaL_where(L, 1);
        lua_pushvalue(L, -2);
        lua_concat(L, 2);
        return lua_error(L);
    }

    // stack: host, port, handler, sd
    assert(lua_gettop(L) == 4);

    struct loop_tcp_cb loop_cb = {
        .error = bind_loop_error_cb,
        .connect = bind_loop_connect_cb,
        .read = bind_loop_read_cb,
        .data = sd
    };
    sd->L = L;
    sd->sock = loop_tcp_connect(host, port, loop_cb);

    if ( !sd->sock ) {
        luaL_unref(L, LUA_REGISTRYINDEX, sd->sd_ref);

        luaL_where(L, 1);
        lua_pushliteral(L, "Couldn't setup TCP connection");
        lua_concat(L, 2);
        return lua_error(L);
    }

    sd->state = SOCKET_CONNECTING;
    
    strncpy(sd->host, host, SOCKET_HOST_LEN);
    sd->host[SOCKET_HOST_LEN-1] = '\0';
    
    sd->port = port;

    lua_pop(L, 4);

    // stack: <empty>
    assert(lua_gettop(L) == 0);

    lua_rawgeti(L, LUA_REGISTRYINDEX, sd->sd_ref);

    // stack: sd
    assert(lua_gettop(L) == 1);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

#define CREATION_LOCATION_LEN 128

struct listener_data {
    lua_State *L;
    struct loop_listener *ll;
    int port;
    int handler_ref;
    int listener_ref;
    char creation_location[CREATION_LOCATION_LEN];
};

static int bind_loop_listener_gc(lua_State *L) {
    struct listener_data *ld = luaL_checkudata(L, 1, "druid listener");
    
    if ( ld->handler_ref != LUA_REFNIL ) {
        luaL_unref(L, LUA_REGISTRYINDEX, ld->handler_ref);
        ld->handler_ref = LUA_REFNIL;
    }

    if ( ld->listener_ref != LUA_REFNIL ) {
        luaL_unref(L, LUA_REGISTRYINDEX, ld->listener_ref);
        ld->listener_ref = LUA_REFNIL;
    }

    if ( ld->ll ) {
        loop_listener_close(ld->ll);
        ld->ll = NULL;
    }

    return 0;
}

static int bind_loop_listener_tostring(lua_State *L) {
    require_exactly(L, 1);

    struct listener_data *ld = luaL_checkudata(L, 1, "druid listener");
    lua_pop(L, 1);

    lua_pushliteral(L, "listener(on port ");
    lua_pushinteger(L, ld->port);
    lua_pushliteral(L, ")");
    lua_concat(L, 3);

    return 1;
}

static int bind_loop_accept_cb(const char *from, struct loop_tcp_cb *cb, struct loop_sockhandle *h, void *data) {
    struct listener_data *ld = data;
    lua_State *L = ld->L;

    struct socket_data *sd = lua_newuserdata(L, sizeof(struct socket_data));
    assert(sd);
    lua_pushvalue(L, -1);
    sd->sd_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    bind_loop_socket_setmetatable(L);

    // stack: sd

    lua_rawgeti(L, LUA_REGISTRYINDEX, ld->handler_ref); // stack: sd, handler
    if ( !bind_loop_sock_setup_cbrefs(L) ) {
        // stack: sd, errmsg

        lua_pushstring(L, ld->creation_location);
        lua_pushvalue(L, -2);
        lua_concat(L, 2);

        // stack: sd, errmsg, where+errmsg

        int r = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_pop(L, 2);
        lua_rawgeti(L, LUA_REGISTRYINDEX, r);
        luaL_unref(L, LUA_REGISTRYINDEX, r);

        // stack: where+errmsg

        //lua_error(L);
        // ARGH HOW TO RAISE THIS ERROR
        const char *error = lua_tostring(L, -1);
        logger(LOG_ERR, "bind", "Couldn't accept socket - handler couldn't be created: %s", error);

        lua_pop(L, 1);

        // stack: <empty>

        return -1; // close the socket
    }

    // stack: sd

    struct loop_tcp_cb loop_cb = {
        .error = bind_loop_error_cb,
        .connect = bind_loop_connect_cb,
        .read = bind_loop_read_cb,
        .data = sd
    };
    *cb = loop_cb;

    sd->L = L;
    sd->sock = h;
    sd->state = SOCKET_CONNECTING; // connect callback will be called soon
    
    strncpy(sd->host, from, SOCKET_HOST_LEN);
    sd->host[SOCKET_HOST_LEN-1] = '\0';
    
    sd->port = 0; // TODO

    // stack: sd

    lua_pop(L, 1);

    return 0;
}

static int bind_loop_tcp_listen(lua_State *L) {
    require_exactly(L, 2);

    int port = luaL_checkint(L, 1);
    luaL_argcheck(L, lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TTABLE, 2,
            "Handler is not a function or table");

    int handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);

    luaL_argcheck(L, port > 0 && port <= 65535, 1, "Port is out of range");

    struct listener_data *ld = lua_newuserdata(L, sizeof(struct listener_data));
    assert(ld);
    lua_pushvalue(L, -1);
    ld->listener_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    ld->handler_ref = handler_ref;
    ld->L = L;
    ld->port = port;

    // stack: ld

    luaL_where(L, 1);
    snprintf(ld->creation_location, CREATION_LOCATION_LEN, "%s", lua_tostring(L, -1));
    lua_pop(L, 1);

    // stack: ld

    if ( (ld->ll = loop_tcp_listen(port, bind_loop_accept_cb, ld)) == NULL ) {
        luaL_unref(L, LUA_REGISTRYINDEX, ld->handler_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, ld->listener_ref);
        lua_pop(L, 1);

        lua_pushnil(L);
        return 1;
    }

    if ( luaL_newmetatable(L, "druid listener") ) {
        int table = lua_gettop(L);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, bind_loop_listener_gc);
        lua_settable(L, table);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, bind_loop_listener_tostring);
        lua_settable(L, table);

        lua_pushliteral(L, "__index");
        if ( luaL_newmetatable(L, "druid listener methods") ) {
            luaL_Reg fns[] = {
                { "stop", bind_loop_listener_gc },
                { "tostring", bind_loop_sock_tostring },
                { NULL, NULL }
            };

            luaL_register(L, NULL, fns);
        }
        lua_settable(L, table);
    }

    lua_setmetatable(L, 1);
    
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

int bind_socket(lua_State *L) {
    require_exactly(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    luaL_Reg reg[] = {
        { "tcp_connect", bind_loop_tcp_connect },
        { "tcp_listen", bind_loop_tcp_listen },
        { NULL, NULL }
    };

    luaL_register(L, NULL, reg);

    return 1;
}

