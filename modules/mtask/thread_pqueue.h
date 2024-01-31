#ifndef THREAD_PQUEUE_H
#define THREAD_PQUEUE_H

#include <stddef.h>
#include "thread.h"
#include "cpu/spinlock.h"

#define THREAD_PQUEUE_GROWTH	4

typedef struct {
	thread** heap;
	size_t size, max_size;
	spinlock lock;
} thread_pqueue;

void thread_pqueue_init(thread_pqueue* q);
void thread_pqueue_reset_overflow(thread_pqueue* q);

void thread_pqueue_push(thread_pqueue* q, thread* thr);
thread* thread_pqueue_pop(thread_pqueue* q);

typedef struct cpu_tree_lnode cpu_tree_lnode;
typedef struct {
	thread_pqueue;

	uint64_t time_slice; // length of a time slice in milliseconds
	uint8_t cpu_num;

	uint64_t last_give_time; // nanosecond timestamp which is compared against current timer value to see if this task switch should give a job
	cpu_tree_lnode* list_ptr; // pointer to the list node that contains the tree, used when re-ordering the list when de-queuing a thread
} thread_sched_pqueue;

void thread_sched_pqueue_heapify(thread_sched_pqueue* q);

void thread_sched_pqueue_push(thread_sched_pqueue* q, thread* thr);
thread* thread_sched_pqueue_pop(thread_sched_pqueue* q);
void thread_sched_pqueue_delete(thread_sched_pqueue* q, thread* thr);

#endif
