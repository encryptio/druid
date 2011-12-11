#ifndef __LOOP_H__
#define __LOOP_H__

typedef void (*loop_timer_cb)(void *data);

void loop_setup(void);
void loop_teardown(void);
void loop_until_done(void);
void loop_exit_early(void);

void loop_add_timer(double in, loop_timer_cb cb, void *data);

#endif
