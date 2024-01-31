#include "scheduler.h"
#include "thread_tree.h"
#include "thread_pqueue.h"

#include "cpu/spinlock.h"
#include "mtask.h"
#include "string.h"
#include "kernlib/kernmem.h"
#include "log/boot_log.h"
#include "dev/uart.h"
#include "cpu/x86/apic.h"
#include "cpu/x86/hpet.h"

#define USE_THREAD_PQUEUE	1

struct cpu_tree_lnode {
#if USE_THREAD_PQUEUE
	thread_sched_pqueue* tree;
#else
	thread_tree* tree;
#endif
	cpu_tree_lnode* next;
	cpu_tree_lnode* prev;
};

#if USE_THREAD_PQUEUE
thread_sched_pqueue* cpu_queues;
#else
thread_tree* cpu_trees;
#endif

static cpu_tree_lnode* cpu_tree_list; // oredered from cpu queue with lowest amount of jobs to highest
static spinlock cpu_tree_list_lock;

static thread_pqueue* cpu_sleep_pqueue_list;

static void print_cpu_tree_list()
{
	uart_printf("\r\n");
	cpu_tree_lnode* cur = cpu_tree_list;
	while(cur){
#if USE_THREAD_PQUEUE
		uart_printf("%p[%u]\tnext: %p\tprev: %p\tthread_cnt: %lu\ttree_ptr: %p\r\n", cur, cur->tree->cpu_num, cur->next, cur->prev, cur->tree->size, cur->tree);
#else
		uart_printf("%p[%u]\tnext: %p\tprev: %p\tthread_cnt: %lu\r\n", cur, cur->tree->cpu_num, cur->next, cur->prev, cur->tree->thread_cnt);
#endif
		cur = cur->next;
	}
	uart_printf("\r\n");
	uart_printf("\r\n");
}

extern uint64_t _ts_scheduler_advance_thread_queue[1];
extern uint64_t _ts_scheduler_prev_threads[1];
thread* scheduler_advance_thread_queue();
thread** scheduler_prev_threads;
thread** scheduler_cur_threads;

static void* timer_addr;
static uint64_t* timer_prev_val;
static uint64_t timer_res_ns;

int scheduler_init()
{
#if USE_THREAD_PQUEUE
	cpu_queues = kmalloc(sizeof(thread_sched_pqueue) * core_num);
#else
	cpu_trees = kmalloc(sizeof(thread_tree) * core_num);
#endif
	cpu_sleep_pqueue_list = kmalloc(sizeof(thread_pqueue) * core_num);

	cpu_tree_list = NULL;
	for(uint8_t i = 0; i < core_num; ++i){
		cpu_tree_lnode* new_lnode = kmalloc(sizeof(cpu_tree_lnode));

#if USE_THREAD_PQUEUE
		thread_sched_pqueue* t = cpu_queues + i;
		thread_pqueue_init((thread_pqueue*)t);
#else
		thread_tree* t = cpu_trees + i;
		t->thread_cnt = 0;
		spinlock_init(&t->lock);
		t->root = NULL;
#endif

		t->time_slice = 0;
		t->cpu_num = i;
		new_lnode->tree = t;

		new_lnode->next = cpu_tree_list;
		new_lnode->prev = NULL;
		if(cpu_tree_list)
			cpu_tree_list->prev = new_lnode;
		cpu_tree_list = new_lnode;
		t->list_ptr = new_lnode;

		thread_pqueue_init(&cpu_sleep_pqueue_list[i]);
	}
	spinlock_init(&cpu_tree_list_lock);

	*_ts_scheduler_advance_thread_queue = (uintptr_t)scheduler_advance_thread_queue;

	scheduler_prev_threads = kmalloc_align(sizeof(thread*) * core_num, SCHEDULER_THREAD_ALIGN);
	for(uint8_t i = 0; i < core_num; ++i)
		scheduler_prev_threads[i] = NULL;
	*_ts_scheduler_prev_threads = (uintptr_t)scheduler_prev_threads;

	scheduler_cur_threads = kmalloc_align(sizeof(thread*) * core_num, SCHEDULER_THREAD_ALIGN);
	for(uint8_t i = 0; i < core_num; ++i)
		scheduler_cur_threads[i] = NULL;

	size_t hpet_timer_blocks_cnt;
	hpet_desc_table** hpet_timer_blocks = hpet_get_timer_blocks(&hpet_timer_blocks_cnt);
	timer_addr = (void*)hpet_timer_blocks[0]->base_addr.addr;
	timer_res_ns = HPET_COUNTER_CLK_PERIOD(HPET_READ_REG(timer_addr, HPET_GENREG_CAP_ID));
	if(timer_res_ns < 1000000) // clock period is in femtoseconds (10^-15), nanoseconds are 10^-9
		boot_log_printf_status(BOOT_LOG_STATUS_WARN, "HPET clock period is less than a nanosecond (%lu fs), using timer value for vruntime instead of nanoseconds", timer_res_ns);
	else
		timer_res_ns /= 1000000;

	timer_prev_val = kmalloc(sizeof(uint64_t) * core_num);
	return 0;
}

void sync_timers()
{
	uint64_t timer_val = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);
	uint64_t spliced_task_give_delay = task_give_delay / core_num;
	if(spliced_task_give_delay < scheduler_latency * 10)
		spliced_task_give_delay = scheduler_latency * 10;
	for(uint8_t i = 0; i < core_num; ++i){
		timer_prev_val[i] = timer_val;
#if USE_THREAD_PQUEUE
		cpu_queues[i].last_give_time = timer_val - spliced_task_give_delay * (i + 1); // oveflow is purely intentional
#else
		cpu_trees[i].last_give_time = timer_val - spliced_task_give_delay * (i + 1); // oveflow is purely intentional
#endif
	}
}

// Call this function if the smallest element has had it's thread count increased and should be moved to a new position.
static void cpu_tree_list_move_smallest()
{
	spinlock_lock(&cpu_tree_list_lock);
	cpu_tree_lnode* next_head = cpu_tree_list->next;
	cpu_tree_lnode* cur = cpu_tree_list->next;
	if(cur){
		cpu_tree_lnode* prev = cur->prev;
#if USE_THREAD_PQUEUE
		while(cur && cpu_tree_list->tree->size > cur->tree->size){
#else
		while(cur && cpu_tree_list->tree->thread_cnt > cur->tree->thread_cnt){
#endif
			prev = cur;
			cur = cur->next;
		}
		if(!cur){ // reached the end of the list, still have the biggest amount of threads
			prev->next = cpu_tree_list;
			cpu_tree_list->next = NULL;
			cpu_tree_list->prev = prev;
			cpu_tree_list = next_head;
		}
		else if(cur->prev != cpu_tree_list){ // if the tree was actually moved, replace head element with 2nd next lowest on threads tree
			cur->prev->next = cpu_tree_list;
			cpu_tree_list->next = cur;
			cpu_tree_list->prev = cur->prev;
			cur->prev = cpu_tree_list;

			cpu_tree_list = next_head;
			next_head->prev = NULL;
		}
	}
	cpu_tree_list->prev = NULL;
	spinlock_unlock(&cpu_tree_list_lock);
}
// If a thread count decreased for a cpu tree, call this function to move it to the left (closer to the fewer thread count trees)
#if USE_THREAD_PQUEUE
static void cpu_tree_list_move_left(thread_sched_pqueue* tree)
#else
static void cpu_tree_list_move_left(thread_tree* tree)
#endif
{
	spinlock_lock(&cpu_tree_list_lock);
	cpu_tree_lnode* cur = tree->list_ptr->prev; // begin at the node previous to the tree in question

	if(cur){ // if the tree is not at the beginning already
#if USE_THREAD_PQUEUE
		while(cur && tree->size < cur->tree->size)
#else
		while(cur && tree->thread_cnt < cur->tree->thread_cnt)
#endif
			cur = cur->prev;
		if(!cur){ // reached the beginning of the list, still have the smallest amount of threads
			/*if(tree->list_ptr->prev) not needed, outer if checks for it already */
			tree->list_ptr->prev->next = tree->list_ptr->next;
			if(tree->list_ptr->next)
				tree->list_ptr->next->prev = tree->list_ptr->prev;

			tree->list_ptr->next = cpu_tree_list;
			tree->list_ptr->prev = NULL;
			cpu_tree_list->prev = tree->list_ptr;
			cpu_tree_list = tree->list_ptr;
		}
		else if(cur->prev != cpu_tree_list){ // got stopped in the middle
			/*if(tree->list_ptr->prev) not needed, outer if checks for it already */
			tree->list_ptr->prev->next = tree->list_ptr->next;
			if(tree->list_ptr->next)
				tree->list_ptr->next->prev = tree->list_ptr->prev;

			tree->list_ptr->next = cur->next;
			tree->list_ptr->prev = cur;
			if(cur->next)
				cur->next->prev = tree->list_ptr;
			cur->next = tree->list_ptr;
		}
	}
	spinlock_unlock(&cpu_tree_list_lock);
}

/* Scheduling functions */

static void endless_loop() { while(1) ; } // this loop is executed until 1st thread is added to the AP that executes it

#if USE_THREAD_PQUEUE
static void thread_tree_add(thread_sched_pqueue* tree, thread* th)
#else
static void thread_tree_add(thread_tree* tree, thread* th)
#endif
{
	uint64_t min_vruntime = 0;
#if USE_THREAD_PQUEUE
	min_vruntime = tree->heap ? tree->heap[0]->vruntime : 0;
	tree->time_slice = scheduler_latency / (tree->size + 1);
#else
	thread_tree_node* root = tree->root;
	if(root){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
		min_vruntime = root->thr->vruntime;
	}
	++tree->thread_cnt;
	tree->time_slice = scheduler_latency / tree->thread_cnt;
#endif
	th->vruntime = min_vruntime;

	if(tree->time_slice < min_granularity)
		tree->time_slice = min_granularity;

	tree->time_slice *= 1000000;
	th->tree = tree;
#if USE_THREAD_PQUEUE
	thread_sched_pqueue_push(tree, th);
#else
	thread_tree_node* n = kmalloc(sizeof(thread_tree_node));
	n->thr = th;
	th->hndl = n; 
	thread_tree_insert(tree, n);
#endif
}
static void thread_tree_remove(thread* th)
{
#if USE_THREAD_PQUEUE
	thread_sched_pqueue* tree = th->tree;
#else
	thread_tree* tree = th->tree;
	--tree->thread_cnt;
	tree->time_slice = tree->thread_cnt ? scheduler_latency / tree->thread_cnt : 0;
#endif

	uint32_t lapic_id = lapic_read(LAPIC_REG_ID) >> 24;
	if(tree->time_slice < min_granularity)
		tree->time_slice = min_granularity;
	tree->time_slice *= 1000000;

#if USE_THREAD_PQUEUE
	thread_sched_pqueue_delete(tree, th);
	tree->time_slice = tree->size ? scheduler_latency / tree->size : 0;
#else
	thread_tree_node* n = thread_tree_delete(tree, th->hndl);
	kfree(n);
#endif
}

#if USE_THREAD_PQUEUE
static uint64_t get_min_vruntime(thread_sched_pqueue* tree)
{
	return tree->size > 0 ? tree->heap[0]->vruntime : 0;
}
#else
static uint64_t get_min_vruntime(thread_tree* tree)
{
	thread_tree_node* root = tree->root;
	if(root){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
		return root->thr->vruntime;
	}
	RETUrn 0;
}
#endif

void scheduler_queue_thread(thread* th)
{
	uart_printf("%p thread queue:\r\n", th);
#if USE_THREAD_PQUEUE
	thread_sched_pqueue* tree = cpu_tree_list->tree;
#else
	thread_tree* tree = cpu_tree_list->tree;
#endif
	spinlock_lock(&tree->lock);
	th->vruntime = get_min_vruntime(tree);
	// Find a tree with least amount of jobs (using binary heap)
	thread_tree_add(tree, th);
	spinlock_unlock(&tree->lock);

	cpu_tree_list_move_smallest();

	// this AP jump is only needed so the core actually gets initialized and starts getting task switch interrupts
	uint8_t core_i = tree->cpu_num;
	if(!(core_info[core_i].flags & MTASK_CORE_FLAG_BSP) && !core_info[core_i].jmp_loc)
		ap_jump(core_i, endless_loop);
}
void scheduler_dequeue_thread(thread* th)
{
	uart_printf("%p thread dequeue %p:\r\n", th, th->tree);
#if USE_THREAD_PQUEUE
	spinlock_lock(&((thread_sched_pqueue*)th->tree)->lock);
	thread_tree_remove(th);
	spinlock_unlock(&((thread_sched_pqueue*)th->tree)->lock);
#else
	spinlock_lock(&((thread_tree*)th->tree)->lock);
	thread_tree_remove(th);
	spinlock_unlock(&((thread_tree*)th->tree)->lock);
#endif
	cpu_tree_list_move_left(th->tree);
}
void scheduler_sleep_thread(thread* th, uint64_t time_ns)
{
	uart_printf("%p thread sleep for %lu ns %p:\r\n", th, time_ns, th->tree);
	time_ns /= timer_res_ns; // convert to timer ticks

	th->flags |= THREAD_FLAG_SLEEPING;

	uint64_t timer_val = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);
	th->sleep_overflow = (uint64_t)-1 - time_ns < timer_val;
	th->sleep_until = timer_val + time_ns;

	scheduler_dequeue_thread(th);
#if USE_THREAD_PQUEUE
	thread_pqueue* q = &cpu_sleep_pqueue_list[((thread_sched_pqueue*)th->tree)->cpu_num];
#else
	thread_pqueue* q = &cpu_sleep_pqueue_list[((thread_tree*)th->tree)->cpu_num];
#endif
	spinlock_lock(&q->lock);
	thread_pqueue_push(q, th);
	spinlock_unlock(&q->lock);
}

/* called in ap_periodic_switch.s */
static uint64_t ts_time_left = 0; // tracks time before an actual task switch
thread* scheduler_advance_thread_queue()
{
	uint32_t lapic_id = lapic_read(LAPIC_REG_ID) >> 24;

	// Measure time passed since last interrupt
	uint64_t timer_val = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);
	uint64_t prev_val = timer_prev_val[lapic_id];
	uint64_t time_passed;
	thread_pqueue* sleep_queue = &cpu_sleep_pqueue_list[lapic_id];
	if(prev_val > timer_val){
		time_passed = (uint64_t)-1 - prev_val + timer_val;
		thread_pqueue_reset_overflow(sleep_queue);
	}
	else
		time_passed = timer_val - prev_val;

	// Check on the earliest-to-wake-up sleeping threads
	while(sleep_queue->size){
		thread* th = sleep_queue->heap[0];
		if(!thread_should_wakeup_now(*th, timer_val))
			break;
		thread_pqueue_pop(sleep_queue);
		scheduler_queue_thread(th);
		/* TEST BEGIN */
		th->last_sched_latency = timer_val - th->sleep_until;
		/* TEST END */
	}

#if USE_THREAD_PQUEUE
	thread_sched_pqueue* tree = &cpu_queues[lapic_id];
#else
	thread_tree* tree = &cpu_trees[lapic_id];
#endif
	spinlock_lock(&tree->lock);
	// Check if it's time to give tasks
	uint64_t time_passed_after_give = tree->last_give_time;
	if(time_passed_after_give > timer_val)
		time_passed_after_give = (uint64_t)-1 - time_passed_after_give + timer_val;
	else
		time_passed_after_give = timer_val - time_passed_after_give;

	if(time_passed_after_give >= task_give_delay){
#if USE_THREAD_PQUEUE
		uint64_t min_count = cpu_tree_list->tree->size;
		uint64_t task_count_diff = tree->size - min_count;
		uint64_t thres_amt = tree->size * task_give_thres / 100;
#else
		uint64_t min_count = cpu_tree_list->tree->thread_cnt;
		uint64_t task_count_diff = tree->thread_cnt - min_count;
		uint64_t thres_amt = tree->thread_cnt * task_give_thres / 100;
#endif
		if(thres_amt < task_give_thres_min)
			thres_amt = task_give_thres_min;
		if(task_count_diff >= thres_amt){ // time to give away some jobs
			uint64_t give_amt = task_count_diff / 2;

			uart_printf("\r\n#%u has to give %lu jobs to #%u (diff %lu)\r\n", lapic_id, give_amt, cpu_tree_list->tree->cpu_num, task_count_diff);
			for(; give_amt != 0; --give_amt){
#if USE_THREAD_PQUEUE
				thread* th = thread_sched_pqueue_pop(tree);
				cpu_tree_list_move_left(tree);
#else
				thread* th = tree->root->thr; // a bit of code duplication but this avoids double locking current CPU tree spinlock
				thread_tree_remove(tree->root->thr);
				cpu_tree_list_move_left(tree);
#endif
				scheduler_queue_thread(th);
			}
		}
		tree->last_give_time = timer_val;
	}

	// Check if time slice allocated for this thread has passed
	if(time_passed < tree->time_slice){
		scheduler_prev_threads[lapic_id] = scheduler_cur_threads[lapic_id];
		//if(lapic_id == 0)
		//	uart_printf("save to thread: %p\r\n", scheduler_prev_threads[lapic_id]);
		//scheduler_prev_threads[lapic_id] = NULL;
		spinlock_unlock(&tree->lock);
		return NULL;
	}
	timer_prev_val[lapic_id] = timer_val;

	/* TEST BEGIN */
	uint64_t ts_time_begin = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);
	/* TEST END */

	// Find current thread
#if USE_THREAD_PQUEUE
	if(!tree->size){
		spinlock_unlock(&tree->lock);
		return NULL;
	}

	thread* th;
	if(!scheduler_cur_threads[lapic_id])
		th = tree->heap[0];
	else
		th = scheduler_cur_threads[lapic_id];

	// Incement it's runtime and re-insert it in the tree
	th->vruntime += time_passed * timer_res_ns * default_weight / th->weight;

	if(th == tree->heap[0]){
		thread_sched_pqueue_pop(tree);
		thread_sched_pqueue_push(tree, th);
	}
	else
		thread_sched_pqueue_heapify(tree);
#else
	thread_tree_node* root = tree->root;
	if(!root){
		spinlock_unlock(&tree->lock);
		return NULL;
	}

	if(!scheduler_cur_threads[lapic_id]){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
	}
	else
		root = scheduler_cur_threads[lapic_id]->hndl;

	// Incement it's runtime and re-insert it in the tree
	root->thr->vruntime += time_passed * timer_res_ns * default_weight / root->thr->weight;
	thread_tree_node root_cpy = *root;

	thread_tree_node* root_addr = thread_tree_delete(tree, root);
	*root_addr = root_cpy;
	thread_tree_insert(tree, root_addr);
#endif

	spinlock_unlock(&tree->lock);

	// Find thread with minimum vruntime
#if USE_THREAD_PQUEUE
	thread* cur_thr = tree->heap[0];
	//uart_printf("cur thr %p %lu\r\n", cur_thr, tree->size);
#else
	root = tree->root;
	while(root->child[TREE_DIR_LEFT])
		root = root->child[TREE_DIR_LEFT];
	thread* cur_thr = root->thr;
#endif
	scheduler_cur_threads[lapic_id] = scheduler_prev_threads[lapic_id] = cur_thr;

	/* TEST BEGIN */
	uint64_t ts_time_end = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);
	uint64_t ts_time = ts_time_end - ts_time_begin;
	++cur_thr->task_switch_cnt;
	cur_thr->task_switch_avg += ts_time;
	if(ts_time > cur_thr->task_switch_max) cur_thr->task_switch_max = ts_time;
	if(cur_thr->task_switch_min == 0 || ts_time < cur_thr->task_switch_min) cur_thr->task_switch_min = ts_time;
	/* TEST END */

	return cur_thr;
}
