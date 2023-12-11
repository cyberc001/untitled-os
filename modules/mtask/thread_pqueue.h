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

#endif
