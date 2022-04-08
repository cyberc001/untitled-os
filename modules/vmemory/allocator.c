#include "allocator.h"

#include <stddef.h>

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#define TREE_CLR_BLACK 	0
#define TREE_CLR_RED 	1

#define TREE_DIR_LEFT	0
#define TREE_DIR_RIGHT	1

#define TREE_DIR_CHILD(n) ((n) == ((n)->parent)->child[TREE_DIR_RIGHT] ? TREE_DIR_RIGHT : TREE_DIR_LEFT)
#define TREE_GET_SIBLING(n) ((n)->parent ? (n)->parent->child[TREE_DIR_CHILD(n)] : NULL)

#define TREE_GET_CLR(n) ((uint64_t)(n)->size >> 63)
#define TREE_SET_CLR(n, clr) { if(clr) (n)->size = ((uint64_t)(n)->size | ((uint64_t)1 << 63)); else (n)->size = ((uint64_t)(n)->size & ~((uint64_t)1 << 63)); }
#define TREE_GET_SIZE(n) ((uint64_t)(n)->size & ~((uint64_t)1 << 63))
#define TREE_SET_SIZE(n, sz) { (n)->size = (sz) & ~((uint64_t)1 << 63); }

typedef struct node node;
struct node {
	void* addr;
	uint64_t size;

	node *child[2];
	node *parent;
};

static struct {
	node* root;
} alloc_tree;


// Memory pool, since allocator uses module loader memory allocation, which relies on paging, which relies on allocator
#define NODE_MEM_POOL_SIZE	112	// doubled rough estimate, considering that there are 48 bits adressable: log_2(memory size / 2 MB (page size)) * 2 ~ 56
static node* node_mem_pool[NODE_MEM_POOL_SIZE];
static size_t node_mem_pool_i;		// current index into memory pool.

static void node_mem_pool_init()
{
	for(size_t i = 0; i < NODE_MEM_POOL_SIZE; ++i)
		node_mem_pool[i] = kmalloc(sizeof(node));
}
static node* alloc_node()
{
	if(node_mem_pool_i++ >= NODE_MEM_POOL_SIZE / 2){
		for(size_t i = node_mem_pool_i; i < NODE_MEM_POOL_SIZE; ++i){ // can't use memmove: it requires memory allocation (at least in a naive implementation)
			node_mem_pool[i - node_mem_pool_i] = node_mem_pool[i];
			node_mem_pool[i] = kmalloc(sizeof(node));
		}
		node_mem_pool_i = 0;
	}
	return node_mem_pool[node_mem_pool_i];
}


void alloc_tree_insert(node* n);
void alloc_tree_insertp(node* n, node* p, int dir);
void alloc_tree_delete(node *n);

node* alloc_tree_rotate(node* p, int dir);

#define alloc_tree_find_first_fit(size) alloc_tree_find_first_fit_r(alloc_tree.root, size)
node* alloc_tree_find_first_fit_r(node* n, uint64_t size);
node* alloc_tree_find(void* addr);
node* alloc_tree_find_containing(void* addr, uint64_t size);

#define alloc_tree_print() alloc_tree_print_r(alloc_tree.root, 0)
void alloc_tree_print_r(node* n, unsigned depth);


// Public interface

void allocator_init(uint64_t memory_limit)
{
	node_mem_pool_init();
	alloc_tree.root = alloc_node();
	alloc_tree.root->addr = (void*)0;
	TREE_SET_SIZE(alloc_tree.root, memory_limit);
	alloc_tree.root->child[0] = alloc_tree.root->child[1] = alloc_tree.root->parent = NULL;
	TREE_SET_CLR(alloc_tree.root, TREE_CLR_BLACK);
}


void* allocator_alloc(uint64_t size)
{
	node* n = alloc_tree_find_first_fit(size);
	if(n == (void*)-1)
		return n;

	if(TREE_GET_SIZE(n) == size)
	{ // perfect fit
		void* ret = n->addr;
		alloc_tree_delete(n);
		return ret;
	}
	else
	{ // n->size > size, shrinking the free space and shifting addr up
		void* ret = n->addr;
		TREE_SET_SIZE(n, TREE_GET_SIZE(n) - size);
		n->addr += size;
		return ret;
	}
}

void* allocator_alloc_addr(uint64_t size, void* addr)
{
	node* n = alloc_tree_find_containing(addr, size);
	if(n == (void*)-1)
		return (void*)-1;

	if(TREE_GET_SIZE(n) == size)
	{ // perfect fit
		alloc_tree_delete(n);
		return addr;
	}
	else
	{ // requested addr might be lying somewhere in [n->addr; n->addr + n->size - size]
		// splitting the node into 2 parts
		void *addr1 = n->addr, *addr2 = addr + size;
		uint64_t size1 = (uint64_t)(addr - n->addr), size2 = (uint64_t)(n->addr + TREE_GET_SIZE(n) - addr - size);
		alloc_tree_delete(n);
		if(size1){
			node* _new = alloc_node();
			_new->addr = addr1; TREE_SET_SIZE(_new, size1);
			alloc_tree_insert(_new);
		}
		if(size2){
			node* _new = alloc_node();
			_new->addr = addr2; TREE_SET_SIZE(_new, size2);
			alloc_tree_insert(_new);
		}
		return addr;
	}
}

node* merge_lr(node* n, void* addr)
{
	while(n){
		if(n->addr + TREE_GET_SIZE(n) == addr)
			return n;
		int dir = addr < n->addr + TREE_GET_SIZE(n) ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		n = n->child[dir];
	}
	return (void*)-1;
}
node* merge_rr(node* n, void* addr, uint64_t size)
{
	while(n){
		if(addr + size == n->addr)
			return n;
		int dir = addr + size < n->addr ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		n = n->child[dir];
	}
	return (void*)-1;
}
void allocator_free(void* addr, uint64_t size)
{
	node* _new = alloc_node();
	_new->addr = addr; TREE_SET_SIZE(_new, size);
	alloc_tree_insert(_new);

	node* merge_l = _new->child[TREE_DIR_LEFT] ? merge_lr(_new->child[TREE_DIR_LEFT], addr) : NULL;
	node* merge_r = _new->child[TREE_DIR_RIGHT] ? merge_rr(_new->child[TREE_DIR_RIGHT], addr, size) : NULL;

	if(merge_l != (void*)-1){
		_new->addr = merge_l->addr;
		TREE_SET_SIZE(_new, TREE_GET_SIZE(_new) + TREE_GET_SIZE(merge_l));
		alloc_tree_delete(merge_l);
	}
	if(merge_r != (void*)-1){
		TREE_SET_SIZE(_new, TREE_GET_SIZE(_new) + TREE_GET_SIZE(merge_r));
		alloc_tree_delete(merge_r);
	}
}


// RB tree functions

void alloc_tree_insert(node* n)
{
	node* root = alloc_tree.root;
	if(!root){
		TREE_SET_CLR(n, TREE_CLR_RED);
		n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
		n->parent = NULL;
		alloc_tree.root = n;
		return;
	}

	while(root)
	{
		int dir = n->addr < root->addr ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		if(!root->child[dir]){
			root->child[dir] = n;
			n->child[0] = n->child[1] = NULL;
			alloc_tree_insertp(n, root, dir);
			return;
		}
		root = root->child[dir];
	}
}
void alloc_tree_insertp(node* n, node* p, int dir)
{
	node	*g,		// grandparent
		*u;		// uncle

	TREE_SET_CLR(n, TREE_CLR_RED)
	n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
	n->parent = p;
	if(!p) // inserting at root
	{ alloc_tree.root = n; return; }

	p->child[dir] = n;

	do{
		// case 1: parent is black, nothing to be done
		if(TREE_GET_CLR(p) == TREE_CLR_BLACK)
			return;
		// parent is certainly red:

		if(!(g = p->parent))
		{ // case 4: parent is red and root
			TREE_SET_CLR(p, TREE_CLR_RED);
			return;
		}

		dir = TREE_DIR_CHILD(p); // at what side p is located in g
		u = g->child[1 - dir];
		if(!u || TREE_GET_CLR(u) == TREE_CLR_BLACK)
		{ // case 5-6: parent is red and uncle is black
			if(n == p->child[1 - dir])
			{ // case 5: parent is red, uncle is black, n is inner grandchild
				alloc_tree_rotate(p, dir);
				n = p;
				p = g->child[dir];
			} // case 5 --> case 6
			// case 6: parent is red, uncle is black, n is outer grandchild
			alloc_tree_rotate(g, 1 - dir);
			TREE_SET_CLR(p, TREE_CLR_BLACK);
			TREE_SET_CLR(g, TREE_CLR_RED);
			return;
		}

		// case 2: parent and uncle are red
		TREE_SET_CLR(p, TREE_CLR_BLACK);
		TREE_SET_CLR(u, TREE_CLR_BLACK);
		TREE_SET_CLR(g, TREE_CLR_RED);
		n = g;
	} while((p = n->parent));
}

node* alloc_tree_delete_replacement(node* n)
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
void alloc_tree_delete_fixbb(node* n) // fix 2 black nodes in a row
{
	if(n == alloc_tree.root)
		return;

	node *s = TREE_GET_SIBLING(n), *p = n->parent;
	if(!s){
		// no sibling - proceed up the tree
		alloc_tree_delete_fixbb(p);
	}
	else{
		if(TREE_GET_CLR(s) == TREE_CLR_RED){
			TREE_SET_CLR(p, TREE_CLR_RED);
			TREE_SET_CLR(s, TREE_CLR_BLACK);
			alloc_tree_rotate(p, TREE_DIR_CHILD(s));
			alloc_tree_delete_fixbb(n);
		}
		else{
			if((s->child[TREE_DIR_LEFT] && TREE_GET_CLR(s->child[TREE_DIR_LEFT]) == TREE_CLR_RED)
			|| (s->child[TREE_DIR_RIGHT] && TREE_GET_CLR(s->child[TREE_DIR_RIGHT]) == TREE_CLR_RED))
			{ // at least 1 red child
				if(s->child[TREE_DIR_LEFT] && TREE_GET_CLR(s->child[TREE_DIR_LEFT]) == TREE_CLR_RED)
				{ // on the left
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left left
						TREE_SET_CLR(s->child[TREE_DIR_LEFT], TREE_GET_CLR(s));
						TREE_SET_CLR(s, TREE_GET_CLR(p));
						alloc_tree_rotate(p, TREE_DIR_RIGHT);
					}
					else
					{ // right left
						TREE_SET_CLR(s->child[TREE_DIR_LEFT], TREE_GET_CLR(p));
						alloc_tree_rotate(s, TREE_DIR_RIGHT);
						alloc_tree_rotate(p, TREE_DIR_LEFT);
					}
				}
				else
				{ // on the right
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left right
						TREE_SET_CLR(s->child[TREE_DIR_RIGHT], TREE_GET_CLR(p));
						alloc_tree_rotate(s, TREE_DIR_LEFT);
						alloc_tree_rotate(p, TREE_DIR_RIGHT);
					}
					else
					{ // right right
						TREE_SET_CLR(s->child[TREE_DIR_RIGHT], TREE_GET_CLR(s));
						TREE_SET_CLR(s, TREE_GET_CLR(p));
						alloc_tree_rotate(p, TREE_DIR_LEFT);
					}
				}
				TREE_SET_CLR(p, TREE_CLR_BLACK);
			}
			else
			{ // 2 black children
				TREE_SET_CLR(s, TREE_CLR_RED);
				if(TREE_GET_CLR(p) == TREE_CLR_BLACK)
					alloc_tree_delete_fixbb(n);
				else
					TREE_SET_CLR(p, TREE_CLR_BLACK);
			}
		}
	}
}
void alloc_tree_delete(node *n)
{
	while(n)
	{
		node* u = alloc_tree_delete_replacement(n);
		int un_is_black = (!u || TREE_GET_CLR(u) == TREE_CLR_BLACK) && TREE_GET_CLR(n) == TREE_CLR_BLACK;

		node* p = n->parent;

		if(!u){
			if(n == alloc_tree.root)
				alloc_tree.root = NULL;
			else{
				if(un_is_black)
					alloc_tree_delete_fixbb(n);
				else
					if(TREE_GET_SIBLING(n))
						TREE_SET_CLR(TREE_GET_SIBLING(n), TREE_CLR_RED);

				// delete n
				p->child[TREE_DIR_CHILD(n)] = NULL;
			}
			kfree(n);
			return;
		}
		if(!n->child[TREE_DIR_LEFT] || !n->child[TREE_DIR_RIGHT]){
			// n has only 1 child
			if(n == alloc_tree.root)
			{ // replace n with it's child if n == root
				n->addr = u->addr; TREE_SET_SIZE(n, TREE_GET_SIZE(u));
				n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
				kfree(u);
			}
			else
			{ // delete n from the tree and move u up
				p->child[TREE_DIR_CHILD(n)] = u;
				u->parent = p;
				if(un_is_black)
					alloc_tree_delete_fixbb(n);
				else
					TREE_SET_CLR(u, TREE_CLR_BLACK);
				kfree(n);
			}
			return;
		}
		// swap u and n
		node tmp;
		tmp.size = TREE_GET_SIZE(n); tmp.addr = n->addr;
		n->size = TREE_GET_SIZE(u); n->addr = u->addr;
		u->size = tmp.size; u->addr = tmp.addr;
		n = u;
	}
}

/* DFS by size */
node* alloc_tree_find_first_fit_r(node* n, uint64_t size)
{
	if(TREE_GET_SIZE(n) >= size)
		return n;

	if(n->child[TREE_DIR_LEFT])
	{ node* nn = alloc_tree_find_first_fit_r(n->child[TREE_DIR_LEFT], size); if(nn) return nn; }
	if(n->child[TREE_DIR_RIGHT])
	{ node* nn = alloc_tree_find_first_fit_r(n->child[TREE_DIR_RIGHT], size); if(nn) return nn; }

	return (void*)-1;
}
/* BT traversal by addr */
node* alloc_tree_find(void* addr)
{
	node* cur = alloc_tree.root;
	while(cur){
		if(cur->addr == addr)
			return cur;
		cur = (addr < cur->addr) ? cur->child[TREE_DIR_LEFT] : cur->child[TREE_DIR_RIGHT];
	}
	return (void*)-1;
}
/* BT traversal by addr, but the criteria is addr + size interval lying in free space */
node* alloc_tree_find_containing(void* addr, uint64_t size)
{
	node* cur = alloc_tree.root;
	while(cur){
		if(addr >= cur->addr && addr + size <= cur->addr + TREE_GET_SIZE(cur))
			return cur;
		cur = (addr < cur->addr) ? cur->child[TREE_DIR_LEFT] : cur->child[TREE_DIR_RIGHT];
	}
	return (void*)-1;
}

// RB tree helper functions

node* alloc_tree_rotate(node* p, int dir)
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
		alloc_tree.root = s;
	return s;
}

void alloc_tree_print_r(node* n, unsigned depth)
{
	for(unsigned i = 0; i < depth; ++i) uart_putchar('\t');
	if(!n){
		uart_printf("--\r\n"); // it's a leaf
		return;
	}
	uart_printf("%c %p $ %p\r\n", TREE_GET_CLR(n) == TREE_CLR_BLACK ? 'B' : 'R', n->addr, (void*)TREE_GET_SIZE(n));

	alloc_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	alloc_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}
