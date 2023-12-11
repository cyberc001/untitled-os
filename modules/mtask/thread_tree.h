#ifndef THREAD_TREE_H
#define THREAD_TREE_H

#include <stddef.h>
#include "thread.h"
#include "spinlock.h"
#include "dev/uart.h"

#define TREE_CLR_BLACK 	0
#define TREE_CLR_RED 	1

#define TREE_DIR_LEFT	0
#define TREE_DIR_RIGHT	1

#define TREE_DIR_CHILD(n) ((n) == ((n)->parent)->child[TREE_DIR_RIGHT] ? TREE_DIR_RIGHT : TREE_DIR_LEFT)
#define TREE_GET_SIBLING(n) ((n)->parent ? (n)->parent->child[TREE_DIR_CHILD(n)] : NULL)

#define thread_tree_print(tree) thread_tree_print_r((tree)->root, 0)

typedef struct node node;
struct node {
	thread* thr;
	unsigned char clr;
	node* child[2];
	node* parent;
};

typedef struct cpu_tree_lnode cpu_tree_lnode;
typedef struct {
	node* root;
	uint64_t thread_cnt;
	uint64_t time_slice; // length of a time slice in milliseconds
	uint8_t cpu_num;

	uint64_t last_give_time; // nanosecond timestamp which is compared against current timer value to see if this task switch should give a job

	cpu_tree_lnode* list_ptr; // pointer to the list node that contains the tree, used when re-ordering the list when de-queuing a thread
	spinlock lock;
} thread_tree;
thread_tree* cpu_trees;

void thread_tree_print_r(node* n, unsigned depth);

void thread_tree_insert(thread_tree* tree, node* n);
node* thread_tree_delete(thread_tree* tree, node* n);


#endif
