#include "loop.h"

#include <stdlib.h>
#include <assert.h>
#include <err.h>

#include <event2/event.h>

#include "logger.h"

////////////////////////////////////////////////////////////////////////////////

static struct event_base *base = NULL;

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

