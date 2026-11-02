#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

struct thread;

#define TIME_SLICE_TICKS 10

void sched_init(void);
void sched_add(struct thread *t);
void sched_remove(struct thread *t);
void sched_unlink_current(void);
void schedule(void);
void sched_block(void);
void sched_wake(struct thread *t);
void sched_sleep(uint64_t ticks);
void sched_check_sleepers(void);
void sched_tick(void);
void sched_idle_loop(void);
void sched_dump(void);
void yield(void);

#endif