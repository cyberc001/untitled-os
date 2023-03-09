#include "scheduler.h"

#include "mtask.h"
#include "string.h"
#include "kernlib/kernmem.h"
#include "dev/uart.h"
#include "cpu/x86/apic.h"

#define TREE_CLR_BLACK 	0
#define TREE_CLR_RED 	1

#define TREE_DIR_LEFT	0
#define TREE_DIR_RIGHT	1

#define TREE_DIR_CHILD(n) ((n) == ((n)->parent)->child[TREE_DIR_RIGHT] ? TREE_DIR_RIGHT : TREE_DIR_LEFT)
#define TREE_GET_SIBLING(n) ((n)->parent ? (n)->parent->child[TREE_DIR_CHILD(n)] : NULL)

#define TREE_FREE_NODE(n) { kfree((n)->thr); kfree(n); }
#define thread_tree_print(tree) thread_tree_print_r((tree)->root, 0)


typedef struct node node;
struct node {
	thread* thr;
	unsigned char clr;
	node* child[2];
	node* parent;
};

typedef struct {
	node* root;
	uint64_t thread_cnt;
} thread_tree;
thread_tree* cpu_trees;
thread_tree** cpu_tree_heap; // organized as a binary heap with fixed size

extern uint64_t _ts_scheduler_advance_thread_queue[1];
extern uint64_t _ts_scheduler_prev_threads[1];
thread* scheduler_advance_thread_queue();
thread** scheduler_prev_threads;

void thread_tree_print_r(node* n, unsigned depth);

int scheduler_init()
{
	cpu_trees = kmalloc(sizeof(thread_tree) * core_num);
	cpu_tree_heap = kmalloc(sizeof(thread_tree*) * core_num);
	for(uint8_t i = 0; i < core_num; ++i){
		thread_tree* t = cpu_trees + i;
		t->thread_cnt = 0;
		t->root = NULL;
		cpu_tree_heap[i] = t;
	}

	*_ts_scheduler_advance_thread_queue = (uintptr_t)scheduler_advance_thread_queue;

	scheduler_prev_threads = kmalloc_align(sizeof(thread*) * core_num, SCHEDULER_THREAD_ALIGN);
	for(uint8_t i = 0; i < core_num; ++i)
		scheduler_prev_threads[i] = NULL;
	*_ts_scheduler_prev_threads = (uintptr_t)scheduler_prev_threads;
	return 0;
}

/* Thread tree binary heap functions */

static void cpu_tree_heap_heapify_up()
{
	size_t k = core_num - 1;
	thread_tree* el = cpu_tree_heap[k];
	for(;;){
		thread_tree* t = cpu_tree_heap[k / 2];
		if(t->thread_cnt < el->thread_cnt) // heap condition is not violated
			break;
		cpu_tree_heap[k] = t; // else drag "el" up
		k /= 2;
		if(k == 0)
			break;
	}
	cpu_tree_heap[k] = el;
}
static void cpu_tree_heap_heapify_down()
{
	thread_tree* el = cpu_tree_heap[0];
	uint8_t tcore_num = core_num - 1;
	size_t k = 0; while(k < tcore_num / 2){
		size_t _min = 0; // minimum child index
		if(k*2 + 1 < tcore_num) // if left child exists
			_min = k*2 + 1;
		if(k*2 + 2 < tcore_num && cpu_tree_heap[k*2 + 2]->thread_cnt < cpu_tree_heap[_min]->thread_cnt) // if right child exists and is smaller than left child
			_min = k*2 + 2;

		thread_tree* t = cpu_tree_heap[_min];
		if(t->thread_cnt >= el->thread_cnt) // heap condition is not violated
			break;
		cpu_tree_heap[k] = t; // else drag "el" down in place of minimum child element
		k = _min;
	}
	cpu_tree_heap[k] = el;
}

// Call this function if the smallest element has had it's thread count increased and should be updated.
static void cpu_tree_heap_update_smallest()
{
	if(core_num == 1)
		return;
	thread_tree* smallest = cpu_tree_heap[0];
	cpu_tree_heap[0] = cpu_tree_heap[core_num - 1];
	cpu_tree_heap_heapify_down();
	cpu_tree_heap[core_num - 1] = smallest;
	cpu_tree_heap_heapify_up();
}

/* RB tree functions */

static node* thread_tree_rotate(thread_tree* tree, node* p, int dir)
{
	node* g = p->parent;
	node* s = p->child[1 - dir];
	node* c = s ? s->child[dir] : NULL;

	p->child[1 - dir] = c;
	if(c) c->parent = p;
	if(s) s->child[dir] = p;
	p->parent = s;
	s->parent = g;

	if(g)
		g->child[p == g->child[TREE_DIR_RIGHT] ? TREE_DIR_RIGHT : TREE_DIR_LEFT] = s;
	else
		tree->root = s;
	return s;
}

static void thread_tree_insertp(thread_tree* tree, node* n, node* p, int dir)
{
	node	*g,		// grandparent
			*u;		// uncle

	n->clr = TREE_CLR_RED;
	n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
	n->parent = p;
	if(!p) // inserting at root
	{ n->clr = TREE_CLR_BLACK; tree->root = n; return; }

	p->child[dir] = n;

	do{
		// case 1: parent is black, nothing to be done
		if(p->clr == TREE_CLR_BLACK)
			return;
		// parent is certainly red:

		if(!(g = p->parent))
		{ // case 4: parent is red and root
		  	p->clr = TREE_CLR_RED;
			return;
		}

		dir = TREE_DIR_CHILD(p); // at what side p is located in g
		u = g->child[1 - dir];
		if(!u || u->clr == TREE_CLR_BLACK)
		{ // case 5-6: parent is red and uncle is black
			if(n == p->child[1 - dir])
			{ // case 5: parent is red, uncle is black, n is inner grandchild
				thread_tree_rotate(tree, p, dir);
				n = p;
				p = g->child[dir];
			} // case 5 --> case 6
			// case 6: parent is red, uncle is black, n is outer grandchild
			thread_tree_rotate(tree, g, 1 - dir);
			p->clr = TREE_CLR_BLACK;
			g->clr = TREE_CLR_RED;
			return;
		}

		// case 2: parent and uncle are red
		p->clr = TREE_CLR_BLACK;
		u->clr = TREE_CLR_BLACK;
		g->clr = TREE_CLR_RED;
		n = g;
	} while((p = n->parent));
}

static void thread_tree_insert(thread_tree* tree, node* n)
{
	node* root = tree->root;
	if(!root){
		n->clr = TREE_CLR_BLACK;
		n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
		n->parent = NULL;
		tree->root = n;
		return;
	}

	while(root)
	{
		int dir = n->thr->vruntime < root->thr->vruntime ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		if(!root->child[dir]){
			root->child[dir] = n;
			n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
			thread_tree_insertp(tree, n, root, dir);
			return;
		}
		root = root->child[dir];
	}
}

static node* thread_tree_delete_replacement(node* n)
{
	if(n->child[TREE_DIR_LEFT] && n->child[TREE_DIR_RIGHT]){
		while(n->child[TREE_DIR_LEFT])
			n = n->child[TREE_DIR_LEFT];
		return n;
	}
	else if(!n->child[TREE_DIR_LEFT] && !n->child[TREE_DIR_RIGHT])
		return NULL;
	else
		return n->child[TREE_DIR_LEFT] ? n->child[TREE_DIR_LEFT] : n->child[TREE_DIR_RIGHT];
}
static void thread_tree_delete_fixbb(thread_tree* tree, node* n) // fix 2 black nodes in a row
{
	if(n == tree->root)
		return;

	node *s = TREE_GET_SIBLING(n), *p = n->parent;
	if(!s){
		// no sibling - proceed up the tree
		thread_tree_delete_fixbb(tree, p);
	}
	else{
		if(s->clr == TREE_CLR_RED){
			p->clr = TREE_CLR_RED;
			s->clr = TREE_CLR_BLACK;
			thread_tree_rotate(tree, p, TREE_DIR_CHILD(s));
			thread_tree_delete_fixbb(tree, n);
		}
		else{
			if((s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->clr  == TREE_CLR_RED)
			|| (s->child[TREE_DIR_RIGHT] && s->child[TREE_DIR_RIGHT]->clr == TREE_CLR_RED))
			{ // at least 1 red child
				if(s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->clr == TREE_CLR_RED)
				{ // on the left
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left left
					  	s->child[TREE_DIR_LEFT]->clr = s->clr;
						s->clr = p->clr;
						thread_tree_rotate(tree, p, TREE_DIR_RIGHT);
					}
					else
					{ // right left
						s->child[TREE_DIR_LEFT]->clr = p->clr;
						thread_tree_rotate(tree, s, TREE_DIR_RIGHT);
						thread_tree_rotate(tree, p, TREE_DIR_LEFT);
					}
				}
				else
				{ // on the right
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left right
						s->child[TREE_DIR_RIGHT]->clr = p->clr;
						thread_tree_rotate(tree, s, TREE_DIR_LEFT);
						thread_tree_rotate(tree, p, TREE_DIR_RIGHT);
					}
					else
					{ // right right
						s->child[TREE_DIR_RIGHT]->clr = s->clr;
						s->clr = p->clr;
						thread_tree_rotate(tree, p, TREE_DIR_LEFT);
					}
				}
				p->clr = TREE_CLR_BLACK;
			}
			else
			{ // 2 black children
				s->clr = TREE_CLR_RED;
				if(p->clr == TREE_CLR_BLACK)
					thread_tree_delete_fixbb(tree, n);
				else
					p->clr = TREE_CLR_BLACK;
			}
		}
	}
}

static void thread_tree_delete(thread_tree* tree, node* n)
{
	while(n)
	{
		node* u = thread_tree_delete_replacement(n);
		int un_is_black = (!u || u->clr == TREE_CLR_BLACK) && n->clr == TREE_CLR_BLACK;

		node* p = n->parent;

		if(!u){
			if(n == tree->root)
				tree->root = NULL;
			else{
				if(un_is_black)
					thread_tree_delete_fixbb(tree, n);
				else
					if(TREE_GET_SIBLING(n))
						TREE_GET_SIBLING(n)->clr = TREE_CLR_RED;

				// delete n
				p->child[TREE_DIR_CHILD(n)] = NULL;
			}
			TREE_FREE_NODE(n);
			return;
		}
		if(!n->child[TREE_DIR_LEFT] || !n->child[TREE_DIR_RIGHT]){
			// n has only 1 child
			if(n == tree->root)
			{ // replace n with it's child if n == root
				n->thr = u->thr;
				n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
				TREE_FREE_NODE(u);
			}
			else
			{ // delete n from the tree and move u up
				p->child[TREE_DIR_CHILD(n)] = u;
				u->parent = p;
				if(un_is_black)
					thread_tree_delete_fixbb(tree, n);
				else
					u->clr = TREE_CLR_BLACK;
				TREE_FREE_NODE(n);
			}
			return;
		}
		// swap u and n
		node tmp;
		tmp.thr = n->thr;
		n->thr = u->thr;
		u->thr = tmp.thr;
		n = u;
	}
}

void thread_tree_print_r(node* n, unsigned depth)
{
	for(unsigned i = 0; i < depth; ++i) uart_putchar('\t');
	if(!n){
		uart_printf("--\r\n"); // it's a leaf
		return;
	}
	uart_printf("%c w %lu vr %lu\r\n", n->clr == TREE_CLR_BLACK ? 'B' : 'R', n->thr->weight, n->thr->vruntime);

	thread_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	thread_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}

/* Scheduling functions */

static void endless_loop() { while(1) ; } // this loop is executed until 1st thread is added to the AP that executes it

static void thread_tree_add(thread_tree* tree, thread* th)
{
	++tree->thread_cnt;
	node* n = kmalloc(sizeof(node));
	n->thr = th;
	thread_tree_insert(tree, n);
}

void scheduler_queue_thread(thread* th)
{
	node* root = cpu_tree_heap[0]->root;
	if(root){
		while(root->child[TREE_DIR_LEFT])
			root = root->child[TREE_DIR_LEFT];
		th->vruntime = root->thr->vruntime;
	}
	else
		th->vruntime = 0;
	// Find a tree with least amount of jobs (using binary heap)
	thread_tree_add(cpu_tree_heap[0], th);
	//thread_tree_print(cpu_tree_heap[0]);
	cpu_tree_heap_update_smallest();
	/*
	// find queue with minimum sum of priorities
	unsigned min_prio = sched_queues[0].priority_sum;
	thread_queue* min_queue = &sched_queues[0];	
	size_t core_i = 0;
	for(size_t i = 1; i < core_num; ++i){
		if(sched_queues[i].priority_sum < min_prio){
			min_prio = sched_queues[i].priority_sum;
			min_queue = &sched_queues[i];
			core_i = i;
		}
	}
	// add the thread to it
	++min_queue->thread_count;
	if(min_queue->thread_count == 1) min_queue->current = 0;
	min_queue->threads = krealloc_align(min_queue->threads, sizeof(thread) * min_queue->thread_count, SCHEDULER_THREAD_ALIGN);
	min_queue->threads[min_queue->thread_count - 1] = *th;
	min_queue->priority_sum += th->parent_proc->priority;
	if(!(core_info[core_i].flags & MTASK_CORE_FLAG_BSP) && !core_info[core_i].jmp_loc)
		ap_jump(core_i, endless_loop);*/
}

/* called in ap_periodic_switch.s */
static uint64_t ts_time_left = 0; // tracks time before an actual task switch
thread* scheduler_advance_thread_queue()
{
	uint32_t lapic_id = lapic_read(LAPIC_REG_ID) >> 24;
	node* root = cpu_trees[lapic_id].root;
	if(!root) return NULL;
	while(root->child[TREE_DIR_LEFT])
		root = root->child[TREE_DIR_LEFT];
	uart_printf("\r\nminimum: %lu\r\n", root->thr->vruntime);

	/*
	// increment current thread index, looping around when hitting the end of the queue
	uint32_t lapic_id = lapic_read(LAPIC_REG_ID) >> 24;
	size_t cur_idx = sched_queues[lapic_id].current + 1;
	if(cur_idx >= sched_queues[lapic_id].thread_count)
		cur_idx = 0;
	if(sched_queues[lapic_id].thread_count == 0)
		return NULL;

	uart_printf("\r\nadvancing to %lu for CPU#%u\r\n", cur_idx, lapic_id);
	sched_queues[lapic_id].current = cur_idx;
	scheduler_prev_threads[lapic_id] = &sched_queues[lapic_id].threads[cur_idx];
	return &sched_queues[lapic_id].threads[cur_idx];*/
	return NULL;
}
