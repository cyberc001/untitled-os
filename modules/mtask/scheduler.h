#ifndef SCHEDULER_H
#define SCHEDULER_H

/* Scheduler for threads. */

#include <stddef.h>

#include "thread.h"
#include "process.h"

#define SCHEDULER_THREAD_ALIGN 	16

// TODO make these parameters be read from a config file
#define scheduler_latency 48
#define min_granularity MTASK_SWITCH_TIMER_TIME
#define default_weight 1024

/* Initializes the scheduler.
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int scheduler_init();

void scheduler_queue_thread(thread* th);
void scheduler_dequeue_thread(thread* th);

/* Changes current thread in the thread queue of the current core to next one.
*  Called by AP_PERIODIC_SWITCH interrupt.
*/
thread* scheduler_advance_thread_queue();

/* Called by toggle_sts() function in mtask.h for syncing timers' previous values. */
void sync_timers();

#endif
