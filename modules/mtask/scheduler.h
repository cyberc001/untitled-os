#ifndef SCHEDULER_H
#define SCHEDULER_H

/* Scheduler for threads.
*  Processes without threads are automatically destroyed.
*/

#include <stddef.h>

#include "thread.h"
#include "process.h"

/* Array that holds all processes in the system. */
extern size_t sched_procs_count;
extern process* sched_procs;

/* Thread queues for each core. */
typedef struct{
	size_t thread_count;
	thread* threads;

	unsigned priority_sum;	// added to when a new thread is added, periodically recalculated when rebalancing
	size_t current;			// index of current thread, there is no current thread if thread_count == 0
} thread_queue;
extern thread_queue* sched_queues;

#define SCHEDULER_THREAD_ALIGN 	16

/* Initializes the scheduler.
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int scheduler_init();

/* Adds a process to the array of all processes.
*  Makes a copy of the process structure rather than storing a pointer.
*  Arguments:
*	 pr - process to add.
*  Return value:
*	pointer to the process entry in the process array.
*/
process* scheduler_add_process(process* pr);

/* Adds a thread to a queue. Tries to initially equalize sum of priorities
*  for each core. ASSUMES THAT THREAD HAS A PARENT PROCESS.
*  Arguments:
*	th = thread to add.
*/
void scheduler_queue_thread(thread* th);

/* Changes current thread in the thread queue of the current core to next one.
*  CALLED BY AP_PERIODIC_SWITCH interrupt.
*/
thread* scheduler_advance_thread_queue();

#endif
