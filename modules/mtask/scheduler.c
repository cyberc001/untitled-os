#include "scheduler.h"
#include "thread_tree.h"
#include "spinlock.h"

#include "mtask.h"
#include "string.h"
#include "kernlib/kernmem.h"
#include "log/boot_log.h"
#include "dev/uart.h"
#include "cpu/x86/apic.h"
#include "cpu/x86/hpet.h"

struct cpu_tree_lnode {
	thread_tree* tree;
	cpu_tree_lnode* next;
	cpu_tree_lnode* prev;
};
static cpu_tree_lnode* cpu_tree_list; // oredered from cpu queue with lowest amount of jobs to highest
static spinlock cpu_tree_list_lock;

static void print_cpu_tree_list()
{
	uart_printf("\r\n");
	cpu_tree_lnode* cur = cpu_tree_list;
	while(cur){
		uart_printf("%p[%u]\tnext: %p\tprev: %p\tthread_cnt: %lu\r\n", cur, cur->tree->cpu_num, cur->next, cur->prev, cur->tree->thread_cnt);
		cur = cur->next;
	}
	uart_printf("\r\n");
	uart_printf("\r\n");
}

extern uint64_t _ts_scheduler_advance_thread_queue[1];
extern uint64_t _ts_scheduler_prev_threads[1];
thread* scheduler_advance_thread_queue();
thread** scheduler_prev_threads;

static void* timer_addr;
static uint64_t* timer_prev_val;
static uint64_t timer_res_ns;

int scheduler_init()
{
	cpu_trees = kmalloc(sizeof(thread_tree) * core_num);
	cpu_tree_list = NULL;
	for(uint8_t i = 0; i < core_num; ++i){
		thread_tree* t = cpu_trees + i;
		t->thread_cnt = 0;
		t->time_slice = 0;
		t->root = NULL;
		t->cpu_num = i;
		spinlock_init(&t->lock);

		cpu_tree_lnode* new_lnode = kmalloc(sizeof(cpu_tree_lnode));
		new_lnode->tree = t;
		new_lnode->next = cpu_tree_list;
		new_lnode->prev = NULL;
		if(cpu_tree_list)
			cpu_tree_list->prev = new_lnode;
		cpu_tree_list = new_lnode;
		t->list_ptr = new_lnode;
	}
	spinlock_init(&cpu_tree_list_lock);

	*_ts_scheduler_advance_thread_queue = (uintptr_t)scheduler_advance_thread_queue;

	scheduler_prev_threads = kmalloc_align(sizeof(thread*) * core_num, SCHEDULER_THREAD_ALIGN);
	for(uint8_t i = 0; i < core_num; ++i)
		scheduler_prev_threads[i] = NULL;
	*_ts_scheduler_prev_threads = (uintptr_t)scheduler_prev_threads;

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
		cpu_trees[i].last_give_time = timer_val - spliced_task_give_delay * (i + 1); // oveflow is purely intentional
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
		while(cur && cpu_tree_list->tree->thread_cnt > cur->tree->thread_cnt){
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
	spinlock_unlock(&cpu_tree_list_lock);
}
// If a thread count decreased for a cpu tree, call this function to move it to the left (closer to the fewer thread count trees)
static void cpu_tree_list_move_left(thread_tree* tree)
{
	spinlock_lock(&cpu_tree_list_lock);
	cpu_tree_lnode* cur = tree->list_ptr->prev;

	if(cur){ // if the tree is not at the beginning already
		while(cur && tree->thread_cnt < cur->tree->thread_cnt)
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

static void thread_tree_add(thread_tree* tree, thread* th)
{
	uint64_t min_vruntime = 0;
	node* root = tree->root;
	if(root){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
		min_vruntime = root->thr->vruntime;
	}
	th->vruntime = min_vruntime;

	++tree->thread_cnt;
	tree->time_slice = scheduler_latency / tree->thread_cnt;
	if(tree->time_slice < min_granularity)
		tree->time_slice = min_granularity;
	tree->time_slice *= 1000000;
	node* n = kmalloc(sizeof(node));
	n->thr = th;
	th->hndl = n; th->tree = tree;
	thread_tree_insert(tree, n);
}
static void thread_tree_remove(thread* th)
{
	thread_tree* tree = th->tree;
	--tree->thread_cnt;
	tree->time_slice = tree->thread_cnt ? scheduler_latency / tree->thread_cnt : 0;
	if(tree->time_slice < min_granularity)
		tree->time_slice = min_granularity;
	tree->time_slice *= 1000000;

	node* n = thread_tree_delete(tree, th->hndl);
	kfree(n);
}

static uint64_t get_min_vruntime(thread_tree* tree)
{
	node* root = tree->root;
	if(root){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
		return root->thr->vruntime;
	}
	return 0;
}

void scheduler_queue_thread(thread* th)
{
	thread_tree* tree = cpu_tree_list->tree;
	spinlock_lock(&tree->lock);
	th->vruntime = get_min_vruntime(tree);
	// Find a tree with least amount of jobs (using binary heap)
	thread_tree_add(tree, th);
	uint8_t core_i = tree->cpu_num;
	spinlock_unlock(&tree->lock);
	cpu_tree_list_move_smallest();

	// this AP jump is only needed so the core actually gets initialized and starts getting task switch interrupts
	if(!(core_info[core_i].flags & MTASK_CORE_FLAG_BSP) && !core_info[core_i].jmp_loc)
		ap_jump(core_i, endless_loop);
	print_cpu_tree_list();
}
void scheduler_dequeue_thread(thread* th)
{
	spinlock_lock(&((thread_tree*)th->tree)->lock);
	thread_tree_remove(th);
	spinlock_unlock(&((thread_tree*)th->tree)->lock);
	cpu_tree_list_move_left(th->tree);
	print_cpu_tree_list();
}
void scheduler_sleep_thread(thread* th)
{

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
	if(prev_val > timer_val)
		time_passed = (uint64_t)-1 - prev_val + timer_val;
	else
		time_passed = timer_val - prev_val;

	thread_tree* tree = &cpu_trees[lapic_id];
	spinlock_lock(&tree->lock);

	// Check if it's time to give tasks
	uint64_t time_passed_after_give = tree->last_give_time;
	if(time_passed_after_give > timer_val)
		time_passed_after_give = (uint64_t)-1 - time_passed_after_give + timer_val;
	else
		time_passed_after_give = timer_val - time_passed_after_give;

	if(time_passed_after_give >= task_give_delay){
		uint64_t min_count = cpu_tree_list->tree->thread_cnt;
		uint64_t task_count_diff = tree->thread_cnt - min_count;
		uint64_t thres_amt = tree->thread_cnt * task_give_thres / 100;
		if(thres_amt < task_give_thres_min)
			thres_amt = task_give_thres_min;
		if(task_count_diff >= thres_amt){ // time to give away some jobs
			uint64_t give_amt = task_count_diff / 2;

			uart_printf("\r\n#%u has to give %lu jobs to #%u (diff %lu)\r\n", lapic_id, give_amt, cpu_tree_list->tree->cpu_num, task_count_diff);
			for(; give_amt != 0; --give_amt){
				thread* th = tree->root->thr; // a bit of code duplication but this avoids double locking current CPU tree spinlock
				thread_tree_remove(tree->root->thr);
				cpu_tree_list_move_left(tree);
				scheduler_queue_thread(th); // this locks minimum thread count CPU tree - actually important
			}

		}
		tree->last_give_time = timer_val;
	}

	// Check if time slice allocated for this thread has passed
	if(time_passed < tree->time_slice){
		scheduler_prev_threads[lapic_id] = NULL;
		spinlock_unlock(&tree->lock);
		return NULL;
	}
	timer_prev_val[lapic_id] = timer_val;

	// Find thread with minimum vruntime
	node* root = tree->root;
	if(!root){
		spinlock_unlock(&tree->lock);
		return NULL;
	}
	while(root->child[TREE_DIR_LEFT])
		root = root->child[TREE_DIR_LEFT];
	// Incement it's runtime and re-insert it in the tree
	root->thr->vruntime += time_passed * timer_res_ns * default_weight / root->thr->weight;
	node root_cpy = *root;
	node* root_addr = thread_tree_delete(tree, root);
	*root_addr = root_cpy;
	thread_tree_insert(tree, root_addr);
	spinlock_unlock(&tree->lock);

	scheduler_prev_threads[lapic_id] = root_addr->thr;
	return root_addr->thr;
}
