#include "thread_pqueue.h"
#include "kernlib/kernmem.h"

static void thread_pqueue_heapify_up(thread_pqueue* q)
{
	size_t k = q->size - 1;
	thread* thr = q->heap[k];
	while(1){
		if(thread_wakeup_is_earlier(*q->heap[k/2], *thr)) // heap condition is satisfied
			break;
		q->heap[k] = q->heap[k/2];
		k /= 2;
		if(k == 0)
			break;
	}
	q->heap[k] = thr;
}
static void thread_pqueue_heapify_down(thread_pqueue* q)
{
	size_t k = 0;
	thread* thr = q->heap[0];
	while(k < q->size / 2){
		if(k*2 + 1 >= q->size)
			break;
		size_t _min = k*2 + (( k*2 + 2 < q->size && thread_wakeup_is_earlier(*q->heap[k*2 + 2], *q->heap[k*2 + 1]) ) ? 2 : 1);
		if(thread_wakeup_is_later(*q->heap[_min], *thr)) // heap condition is satisfied
			break;
		q->heap[k] = q->heap[_min];
		k = _min;
	}
	q->heap[k] = thr;
}

void thread_pqueue_init(thread_pqueue* q)
{
	q->size = q->max_size = 0;
	q->heap = NULL;
	spinlock_init(&q->lock);
}
void thread_pqueue_reset_overflow(thread_pqueue* q)
{
	for(size_t i = 0; i < q->size; ++i)
		q->heap[i]->sleep_overflow = 0;
}

void thread_pqueue_push(thread_pqueue* q, thread* thr)
{
	if(q->size == q->max_size){
		q->max_size += THREAD_PQUEUE_GROWTH;
		q->heap = krealloc(q->heap, sizeof(thread*) * q->max_size);
	}
	q->heap[q->size++] = thr;
	thread_pqueue_heapify_up(q);
}
thread* thread_pqueue_pop(thread_pqueue* q)
{
	thread* thr = q->heap[0];
	q->heap[0] = q->heap[--q->size];
	thread_pqueue_heapify_down(q);
	return thr;
}
