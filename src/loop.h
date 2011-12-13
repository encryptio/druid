#ifndef __LOOP_H__
#define __LOOP_H__

#include <inttypes.h>
#include <unistd.h> // for size_t

struct loop_sockhandle;

void loop_setup(void);
void loop_teardown(void);
void loop_until_done(void);
void loop_exit_early(void);

typedef void (*loop_timer_cb)(void *data);
void loop_add_timer(double in, loop_timer_cb cb, void *data);
void loop_do_repeatedly_whenever(loop_timer_cb cb, void *data);

typedef void (*loop_error_cb)(int err, struct loop_sockhandle *h, void *data);
typedef void (*loop_connect_cb)(struct loop_sockhandle *h, void *data);
typedef void (*loop_read_cb)(size_t in_buffer, struct loop_sockhandle *h, void *data);

struct loop_sockhandle *loop_tcp_connect(const char *host, uint16_t port,
        loop_error_cb cb_err,
        loop_connect_cb cb_connect,
        loop_read_cb cb_read,
        void *data);

size_t loop_sock_peek(struct loop_sockhandle *h, uint8_t *into, size_t want);
void loop_sock_drop(struct loop_sockhandle *h, size_t drop);
void loop_sock_write(struct loop_sockhandle *h, const uint8_t *from, size_t send);
void loop_sock_close(struct loop_sockhandle *h);

#endif
