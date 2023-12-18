#ifndef SCHEDULER_H
#define SCHEDULER_H

/* Scheduler for threads. */

#include <stddef.h>

#include "thread.h"
#include "process.h"

#define SCHEDULER_THREAD_ALIGN 	16

// TODO make these parameters be read from a config file
#define scheduler_latency 10
#define min_granularity 10 /*MTASK_SWITCH_TIMER_TIME*/
#define default_weight 1024
#define task_give_delay 1000000 * 500					// delay between giving away tasks, in ns
#define task_give_thres 15								// threshold on difference between CPU task count and minimum task count, in percents of CPU task count
#define task_give_thres_min 1							// minimum task_give_thres, in tasks count

/* Initializes the scheduler.
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int scheduler_init();

void scheduler_queue_thread(thread* th);
void scheduler_dequeue_thread(thread* th);
void scheduler_sleep_thread(thread* th, uint64_t time_ns);

/* Changes current thread in the thread queue of the current core to next one.
*  Called by AP_PERIODIC_SWITCH interrupt.
*/
thread* scheduler_advance_thread_queue();

/* Called by toggle_sts() function in mtask.h for syncing timers' previous values. */
void sync_timers();

#endif
