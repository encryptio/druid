#include "loop.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include <event2/dns.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "logger.h"

////////////////////////////////////////////////////////////////////////////////

static struct event_base *base = NULL;
static struct evdns_base *base_dns = NULL;
static int base_dns_users = 0;

////////////////////////////////////////////////////////////////////////////////
// evdns management
//
// For some reason, evdns_base objects keep their event_bases alive, even if
// there's nothing for them to do.
//
// I cannot describe how fucking retarded that is.
//
// Work around it by allocating base_dns when wanted and deallocating it when
// unused.

static void evdns_refcount_increment(void) {
    assert(base);

    if ( base_dns_users == 0 ) {
        assert(base_dns == NULL);
        base_dns = evdns_base_new(base, 1);
        assert(base_dns != NULL);
    }

    base_dns_users++;
}

static void evdns_refcount_decrement(void) {
    assert(base);
    assert(base_dns);

    base_dns_users--;
    assert(base_dns_users >= 0);

    if ( base_dns_users == 0 ) {
        evdns_base_free(base_dns, 1);
        base_dns = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

static void loop_log_cb(int severity, const char *msg) {
    int logseverity;
    switch ( severity ) {
        case _EVENT_LOG_DEBUG:
            logseverity = LOG_JUNK;
            break;
        case _EVENT_LOG_MSG:
            logseverity = LOG_INFO;
            break;
        case _EVENT_LOG_WARN:
            logseverity = LOG_WARN;
            break;
        case _EVENT_LOG_ERR:
            logseverity = LOG_ERR;
            break;
        default:
            logseverity = -1; // unknown
            break;
    }

    logger(logseverity, "loop", "libevent: %s", msg);
}

static void loop_fatal_cb(int err) {
    logger(LOG_ERR, "loop", "Got fatal libevent error code %d", err);
    abort();
}

////////////////////////////////////////////////////////////////////////////////
// timers

struct loop_timer { loop_timer_cb cb; void *data; struct event *e; };

static void loop_timeout_cb(evutil_socket_t fd, short what, void *data) {
    struct loop_timer *t = data;
    event_free(t->e);

    // grab the data and free, in case the callback errors
    loop_timer_cb cb = t->cb;
    void *d = t->data;
    free(t);

    cb(d);
}

void loop_add_timer(double in, loop_timer_cb cb, void *data) {
    struct loop_timer *t;
    if ( (t = malloc(sizeof(struct loop_timer))) == NULL )
        err(1, "Couldn't allocate space for loop_timer");
    t->cb = cb;
    t->data = data;

    struct timeval when;
    when.tv_sec = in;
    when.tv_usec = ((double) in - when.tv_sec) * 1000000;
    if ( when.tv_usec < 0 ) when.tv_usec = 0; // stupid floating point

    t->e = evtimer_new(base, loop_timeout_cb, t);
    evtimer_add(t->e, &when);
}

////////////////////////////////////////////////////////////////////////////////
// TCP IO

struct loop_sockhandle {
    struct bufferevent *bev;

    loop_error_cb cb_err;
    loop_connect_cb cb_connect;
    loop_read_cb cb_read;
    void *cb_data;
};

size_t loop_sock_peek(struct loop_sockhandle *h, uint8_t *into, size_t want) {
    ev_ssize_t ret = evbuffer_copyout(bufferevent_get_input(h->bev), into, want);
    if ( ret < 0 )
        logger(LOG_ERR, "loop", "Couldn't peek from socket, ret=%lld", (long long) ret);
    return ret;
}

void loop_sock_drop(struct loop_sockhandle *h, size_t drop) {
    int err = evbuffer_drain(bufferevent_get_input(h->bev), drop);
    if ( err )
        logger(LOG_ERR, "loop", "Couldn't drop from socket, err=%d", err);
}

void loop_sock_write(struct loop_sockhandle *h, const uint8_t *from, size_t send) {
    int err = evbuffer_add(bufferevent_get_output(h->bev), from, send);
    if ( err )
        logger(LOG_ERR, "loop", "Couldn't add data to socket output buffer, err=%d", err);
}

void loop_sock_close(struct loop_sockhandle *h) {
    // TODO: what if callbacks are still pending when this is called?
    //       i hope it doesn't segfault on loop_sock_cb_*
    bufferevent_free(h->bev);
    free(h);
}

////////////////////////////////////////////////////////////////////////////////
// TCP IO callbacks from libevent

static void loop_sock_cb_read(struct bufferevent *bev, void *ctx) {
    struct loop_sockhandle *h = ctx;
    if ( h->cb_read ) {
        size_t size = evbuffer_get_length(bufferevent_get_input(h->bev));
        h->cb_read(size, h, h->cb_data);
    }
}

static void loop_sock_cb_event(struct bufferevent *bev, short events, void *ctx) {
    struct loop_sockhandle *h = ctx;
    if ( events & BEV_EVENT_CONNECTED ) {
        evdns_refcount_decrement();
        logger(LOG_JUNK, "loop", "Connected TCP socket");

        if ( h->cb_connect )
            h->cb_connect(h, h->cb_data);

    } else if ( events & (BEV_EVENT_ERROR | BEV_EVENT_EOF) ) {
        if ( events & BEV_EVENT_ERROR ) {
            int err = bufferevent_socket_get_dns_error(bev);
            if ( err )
                logger(LOG_WARN, "loop", "Couldn't resolve hostname: %s", evutil_gai_strerror(err));

            evdns_refcount_decrement();
        }

        logger(LOG_WARN, "loop", "Couldn't connect to host: %s", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));

        // TODO: actual error codes
        if ( h->cb_err )
            h->cb_err(-1, h, h->cb_data);

    } else {
        logger(LOG_WARN, "loop", "Unhandled callback in loop_sock_cb_event, events = %d", (int)events);
    }
}

////////////////////////////////////////////////////////////////////////////////
// TCP client

struct loop_sockhandle *loop_tcp_connect(const char *host, uint16_t port,
        loop_error_cb cb_err,
        loop_connect_cb cb_connect,
        loop_read_cb cb_read,
        void *data) {

    struct loop_sockhandle *h = NULL;
    if ( (h = calloc(1, sizeof(struct loop_sockhandle))) == NULL )
        err(1, "Couldn't allocate space for loop_sockhandle");

    h->cb_err     = cb_err;
    h->cb_connect = cb_connect;
    h->cb_read    = cb_read;
    h->cb_data    = data;

    if ( (h->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE)) == NULL ) {
        logger(LOG_ERR, "loop", "Couldn't create bufferevent in loop_tcp_connect");
        goto BAD_END;
    }

    bufferevent_setcb(h->bev, loop_sock_cb_read, NULL, loop_sock_cb_event, h);
    bufferevent_enable(h->bev, EV_READ|EV_WRITE);

    evdns_refcount_increment();
    if ( bufferevent_socket_connect_hostname(h->bev, base_dns, AF_UNSPEC, host, port) ) {
        logger(LOG_ERR, "loop", "Couldn't start connecting to %s:%d", host, (int)port);
        goto BAD_END;
    }

    return h;

BAD_END:
    if ( h ) {
        if ( h->bev ) bufferevent_free(h->bev);
    }
    free(h);
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void loop_setup(void) {
    event_set_log_callback(loop_log_cb);
    event_set_fatal_callback(loop_fatal_cb);

    event_enable_debug_mode(); // TODO: remove me

    logger(LOG_JUNK, "loop", "Using libevent version %s", event_get_version());

    base = event_base_new();
    assert(base != NULL);

    logger(LOG_JUNK, "loop", "Using libevent backend %s", event_base_get_method(base));
}

void loop_exit_early(void) {
    event_base_loopbreak(base);
}

void loop_until_done(void) {
    event_base_loop(base, 0);
}

void loop_teardown(void) {
    event_base_free(base);
}

