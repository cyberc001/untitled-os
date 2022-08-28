#include "scheduler.h"

#include "mtask.h"
#include "string.h"
#include "kernlib/kernmem.h"
#include "dev/uart.h"
#include "cpu/x86/apic.h"

size_t sched_procs_count = 0;
process* sched_procs = NULL;

thread_queue* sched_queues = NULL;

extern uint64_t _ts_scheduler_advance_thread_queue[1];
void scheduler_advance_thread_queue();

int scheduler_init()
{
	sched_procs_count = 0; sched_procs = NULL;
	sched_queues = kmalloc(sizeof(thread_queue) * core_num);
	for(size_t i = 0; i < core_num; ++i){
		sched_queues[i].thread_count = 0; sched_queues[i].threads = NULL;
		sched_queues[i].priority_sum = 0; sched_queues[i].current = 0;
	}

	*_ts_scheduler_advance_thread_queue = (uintptr_t)scheduler_advance_thread_queue;
	return 0;
}

process* scheduler_add_process(process* pr)
{
	++sched_procs_count;
	sched_procs = krealloc(sched_procs, sched_procs_count * sizeof(process));
	sched_procs[sched_procs_count - 1] = *pr;
	return &sched_procs[sched_procs_count - 1];
}

void scheduler_queue_thread(thread* th)
{
	// find queue with minimum sum of priorities
	unsigned min_prio = sched_queues[0].priority_sum;
	thread_queue* min_queue = &sched_queues[0];
	for(size_t i = 1; i < core_num; ++i){
		if(sched_queues[i].priority_sum < min_prio){
			min_prio = sched_queues[i].priority_sum;
			min_queue = &sched_queues[i];
		}
	}
	// add the thread to it
	++min_queue->thread_count;
	min_queue->threads = krealloc(min_queue->threads, sizeof(thread) * min_queue->thread_count);
	min_queue->threads[min_queue->thread_count - 1] = *th;
	min_queue->priority_sum += th->parent_proc->priority;
	if(min_queue->thread_count == 1)
		min_queue->current = 0;
}

void scheduler_advance_thread_queue()
{
	// increment current thread index, looping around when hitting the end of the queue
	uint32_t lapic_id = lapic_read(LAPIC_REG_ID);
	if(sched_queues[lapic_id].thread_count == 0)
		return; // no threads: just idle
	size_t prev_idx = sched_queues[lapic_id].current;
	size_t cur_idx = sched_queues[lapic_id].current + 1;
	if(cur_idx >= sched_queues[lapic_id].thread_count)
		cur_idx = 0;
	sched_queues[lapic_id].current = cur_idx;
	// switch to the current thread
	thread* prev = &sched_queues[lapic_id].threads[prev_idx];
	thread* cur = &sched_queues[lapic_id].threads[cur_idx];
	void *save_context_pt = save_context, *load_context_pt = load_context;
	MTASK_CALL_CONTEXT_FUNC(save_context_pt, prev);
	MTASK_CALL_CONTEXT_FUNC(load_context_pt, cur);
}
