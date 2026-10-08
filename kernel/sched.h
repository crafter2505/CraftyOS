#ifndef SCHED_H
#define SCHED_H
#include "types.h"
#include "thread.h"

#define TIME_SLICE_TICKS 10

void sched_init(void);
void sched_add(struct thread *t);
void sched_remove(struct thread *t);
void schedule(void);
void sched_tick(void);
void sched_dump(void);
void yield(void);
void sched_idle_loop(void) __attribute__((noreturn));
void sched_unlink_current(void);

#endif