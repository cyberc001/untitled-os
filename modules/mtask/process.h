#ifndef PROCESS_H
#define PROCESS_H

/* Process API.
*/

#include <stddef.h>
#include "thread.h"

typedef struct process process;
struct process{
	void* memory_hndl;	// handler size is derived from get_mem_hndl_size()

	size_t thread_cnt;
	thread* threads;

	unsigned priority;	// the more priority the more time slices it's threads recieve.
						// taken into account when first adding a thread or rebalancing queues.
};

/* Creates a new process that doesn't contain anything (but has a valid memory handle).
*  Arguments:
*	pr - process instance to initialize.
*/
void create_process(process* pr);

/* Adds a thread to the process. If scheduler is already aware of the process,
*  it will be aware of the thread as well.
*  Makes a copy of the thread structure rather than storing a pointer.
*  Arguments:
*	pr - process to add thread \th\ to.
*  Return value:
*	pointer to thread entry in the thread array of the process.
*/
thread* process_add_thread(process* pr, thread* th);

#endif
