#include "loop.h"

#include <stdbool.h>
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
    struct loop_tcp_cb cb;
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
    size_t size = evbuffer_get_length(bufferevent_get_input(h->bev));
    h->cb.read(size, h, h->cb.data);
}

static void loop_sock_cb_event(struct bufferevent *bev, short events, void *ctx) {
    struct loop_sockhandle *h = ctx;
    if ( events & BEV_EVENT_CONNECTED ) {
        evdns_refcount_decrement();
        logger(LOG_JUNK, "loop", "Connected TCP socket");

        if ( h->cb.connect )
            h->cb.connect(h, h->cb.data);

    } else if ( events & BEV_EVENT_ERROR ) {
        logger(LOG_WARN, "loop", "Couldn't connect to host: %s", evutil_socket_error_to_string(evutil_socket_geterror(bufferevent_getfd(bev))));

        int err = bufferevent_socket_get_dns_error(bev);
        if ( err )
            logger(LOG_WARN, "loop", "Couldn't resolve hostname: %s", evutil_gai_strerror(err));

        evdns_refcount_decrement();

        // TODO: actual error codes
        h->cb.error(-1, h, h->cb.data);

    } else if ( events & BEV_EVENT_EOF ) {
        h->cb.error(0, h, h->cb.data);

    } else {
        logger(LOG_WARN, "loop", "Unhandled callback in loop_sock_cb_event, events = %d", (int)events);
    }
}

////////////////////////////////////////////////////////////////////////////////
// TCP client

struct loop_sockhandle *loop_tcp_connect(const char *host, uint16_t port, struct loop_tcp_cb cb) {
    struct loop_sockhandle *h = NULL;
    if ( (h = calloc(1, sizeof(struct loop_sockhandle))) == NULL )
        err(1, "Couldn't allocate space for loop_sockhandle");

    h->cb = cb;
    assert(cb.read);
    assert(cb.error);
    // cb.connect is optional

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

struct loop_watcher {
    int fd;
    loop_timer_cb cb;
    void *cb_data;

    struct event *ev;
    bool active;
};

static void loop_watch_fd_cb(evutil_socket_t fd, short what, void *data) {
    struct loop_watcher *w = data;
    w->cb(w->cb_data);
}

struct loop_watcher *loop_watch_fd_for_reading(int fd, loop_timer_cb cb, void *data) {
    struct loop_watcher *w;
    if ( (w = malloc(sizeof(struct loop_watcher))) == NULL )
        err(1, "Couldn't malloc space for loop_watcher");

    w->fd      = fd;
    w->cb      = cb;
    w->cb_data = data;
    w->active  = true;

    w->ev = event_new(base, fd, EV_READ|EV_PERSIST, loop_watch_fd_cb, w);
    assert(w->ev);

    event_add(w->ev, NULL);

    return w;
}

void loop_stop_watching(struct loop_watcher *w) {
    assert(w->active);

    event_free(w->ev);
    w->active = false;
}

////////////////////////////////////////////////////////////////////////////////

static loop_timer_cb *whenever_fns = NULL;
static void **whenever_datas       = NULL;
static int whenever_fns_count      = 0;

void loop_do_repeatedly_whenever(loop_timer_cb cb, void *data) {
    whenever_fns_count++;

    if ( (whenever_fns = realloc(whenever_fns, whenever_fns_count*sizeof(loop_timer_cb))) == NULL )
        err(1, "Couldn't realloc space for %d whenever functions", whenever_fns_count);
    if ( (whenever_datas = realloc(whenever_datas, whenever_fns_count*sizeof(void *))) == NULL )
        err(1, "Couldn't realloc space for %d whenever datas", whenever_fns_count);

    whenever_fns[whenever_fns_count-1] = cb;
    whenever_datas[whenever_fns_count-1] = data;
}

void loop_setup(void) {
    event_set_log_callback(loop_log_cb);
    event_set_fatal_callback(loop_fatal_cb);

    event_enable_debug_mode(); // TODO: remove me

    logger(LOG_JUNK, "loop", "Using libevent version %s", event_get_version());

    base = event_base_new();
    assert(base != NULL);

    logger(LOG_JUNK, "loop", "Using libevent backend %s", event_base_get_method(base));
}

static bool loop_exiting_early = false;
void loop_exit_early(void) {
    event_base_loopbreak(base);
    loop_exiting_early = true;
}

void loop_until_done(void) {
    while ( !loop_exiting_early ) {
        int ret = event_base_loop(base, EVLOOP_ONCE);

        if ( ret == -1 ) {
            logger(LOG_ERR, "loop", "event_base_loop returned -1");
            loop_exiting_early = true;
            break;
        } else if ( ret == 1 ) {
            // no events
            break;
        }

        for (int i = 0; i < whenever_fns_count; i++)
            whenever_fns[i](whenever_datas[i]);
    }
    loop_exiting_early = false;
}

void loop_teardown(void) {
    event_base_free(base);
}

