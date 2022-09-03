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
extern uint64_t _ts_scheduler_prev_threads[1];
thread* scheduler_advance_thread_queue();
thread** scheduler_prev_threads;

int scheduler_init()
{
	sched_procs_count = 0; sched_procs = NULL;
	sched_queues = kmalloc(sizeof(thread_queue) * core_num);
	for(size_t i = 0; i < core_num; ++i){
		sched_queues[i].thread_count = 0; sched_queues[i].threads = NULL;
		sched_queues[i].priority_sum = 0; sched_queues[i].current = 0;
	}
	*_ts_scheduler_advance_thread_queue = (uintptr_t)scheduler_advance_thread_queue;
	scheduler_prev_threads = kmalloc_align(sizeof(thread*) * core_num, SCHEDULER_THREAD_ALIGN);
	for(size_t i = 0; i < core_num; ++i)
		scheduler_prev_threads[i] = NULL;
	*_ts_scheduler_prev_threads = (uintptr_t)scheduler_prev_threads;
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
	// TEST
	{
		thread_queue* min_queue = &sched_queues[0];
		size_t min_i = 0;
		++min_queue->thread_count;
		min_queue->threads = krealloc_align(min_queue->threads, sizeof(thread) * min_queue->thread_count, SCHEDULER_THREAD_ALIGN);
		min_queue->threads[min_queue->thread_count - 1] = *th;
		min_queue->priority_sum += th->parent_proc->priority;
		if(min_queue->thread_count == 1)
			min_queue->current = 0;
		if(min_queue->thread_count > 1){
			toggle_sts(1);
			//scheduler_prev_threads[min_i] = &min_queue->threads[0];
		}
		return;
	}
	// TEST

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
	min_queue->threads = krealloc_align(min_queue->threads, sizeof(thread) * min_queue->thread_count, SCHEDULER_THREAD_ALIGN);
	min_queue->threads[min_queue->thread_count - 1] = *th;
	min_queue->priority_sum += th->parent_proc->priority;
	if(min_queue->thread_count == 1)
		min_queue->current = 0;
}

/* called in ap_periodic_switch.s */
thread* scheduler_advance_thread_queue()
{
	// increment current thread index, looping around when hitting the end of the queue
	uint32_t lapic_id = lapic_read(LAPIC_REG_ID);
	size_t cur_idx = sched_queues[lapic_id].current + 1;
	if(cur_idx >= sched_queues[lapic_id].thread_count)
		cur_idx = 0;
	sched_queues[lapic_id].current = cur_idx;
	scheduler_prev_threads[lapic_id] = &sched_queues[lapic_id].threads[cur_idx];
	return &sched_queues[lapic_id].threads[cur_idx];
}
