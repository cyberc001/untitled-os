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

typedef struct node node;
struct node {
	void* addr;
	uint64_t size;

	node *child[2];
	node *parent;
	uint8_t color;
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
	alloc_tree.root->size = memory_limit;
	alloc_tree.root->child[0] = alloc_tree.root->child[1] = alloc_tree.root->parent = NULL;
	alloc_tree.root->color = TREE_CLR_BLACK;
}


void* allocator_alloc(uint64_t size)
{
	node* n = alloc_tree_find_first_fit(size);
	if(!n)
		return NULL;

	if(n->size == size)
	{ // perfect fit
		void* ret = n->addr;
		alloc_tree_delete(n);
		return ret;
	}
	else
	{ // n->size > size, shrinking the free space and shifting addr up
		void* ret = n->addr;
		n->size -= size;
		n->addr += size;
		return ret;
	}
}

void* allocator_alloc_addr(uint64_t size, void* addr)
{
	node* n = alloc_tree_find_containing(addr, size);
	if(!n)
		return NULL;

	if(n->size == size)
	{ // perfect fit
		alloc_tree_delete(n);
		return addr;
	}
	else
	{ // requested addr might be lying somewhere in [n->addr; n->addr + n->size - size]
		// splitting the node into 2 parts
		void *addr1 = n->addr, *addr2 = addr + size;
		uint64_t size1 = (uint64_t)(addr - n->addr), size2 = (uint64_t)(n->addr + n->size - addr - size);
		alloc_tree_delete(n);
		if(size1){
			node* _new = alloc_node();
			_new->addr = addr1; _new->size = size1;
			alloc_tree_insert(_new);
		}
		if(size2){
			node* _new = alloc_node();
			_new->addr = addr2; _new->size = size2;
			alloc_tree_insert(_new);
		}
		return addr;
	}
}

node* merge_lr(node* n, void* addr)
{
	while(n){
		if(n->addr + n->size == addr)
			return n;
		int dir = addr < n->addr + n->size ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		n = n->child[dir];
	}
	return NULL;
}
node* merge_rr(node* n, void* addr, uint64_t size)
{
	while(n){
		if(addr + size == n->addr)
			return n;
		int dir = addr + size < n->addr ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		n = n->child[dir];
	}
	return NULL;
}
void allocator_free(void* addr, uint64_t size)
{
	node* _new = alloc_node();
	_new->addr = addr; _new->size = size;
	alloc_tree_insert(_new);

	node* merge_l = _new->child[TREE_DIR_LEFT] ? merge_lr(_new->child[TREE_DIR_LEFT], addr) : NULL;
	node* merge_r = _new->child[TREE_DIR_RIGHT] ? merge_rr(_new->child[TREE_DIR_RIGHT], addr, size) : NULL;

	if(merge_l){
		_new->addr = merge_l->addr;
		_new->size += merge_l->size;
		alloc_tree_delete(merge_l);
	}
	if(merge_r){
		_new->size += merge_r->size;
		alloc_tree_delete(merge_r);
	}
}


// RB tree functions

void alloc_tree_insert(node* n)
{
	node* root = alloc_tree.root;
	if(!root){
		n->color = TREE_CLR_RED;
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

	n->color = TREE_CLR_RED;
	n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
	n->parent = p;
	if(!p) // inserting at root
	{ alloc_tree.root = n; return; }

	p->child[dir] = n;

	do{
		// case 1: parent is black, nothing to be done
		if(p->color == TREE_CLR_BLACK)
			return;
		// parent is certainly red:

		if(!(g = p->parent))
		{ // case 4: parent is red and root
			p->color = TREE_CLR_RED;
			return;
		}

		dir = TREE_DIR_CHILD(p); // at what side p is located in g
		u = g->child[1 - dir];
		if(!u || u->color == TREE_CLR_BLACK)
		{ // case 5-6: parent is red and uncle is black
			if(n == p->child[1 - dir])
			{ // case 5: parent is red, uncle is black, n is inner grandchild
				alloc_tree_rotate(p, dir);
				n = p;
				p = g->child[dir];
			} // case 5 --> case 6
			// case 6: parent is red, uncle is black, n is outer grandchild
			alloc_tree_rotate(g, 1 - dir);
			p->color = TREE_CLR_BLACK;
			g->color = TREE_CLR_RED;
			return;
		}

		// case 2: parent and uncle are red
		p->color = TREE_CLR_BLACK;
		u->color = TREE_CLR_BLACK;
		g->color = TREE_CLR_RED;
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
		if(s->color == TREE_CLR_RED){
			p->color = TREE_CLR_RED;
			s->color = TREE_CLR_BLACK;
			alloc_tree_rotate(p, TREE_DIR_CHILD(s));
			alloc_tree_delete_fixbb(n);
		}
		else{
			if((s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->color == TREE_CLR_RED)
			|| (s->child[TREE_DIR_RIGHT] && s->child[TREE_DIR_RIGHT]->color == TREE_CLR_RED))
			{ // at least 1 red child
				if(s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->color == TREE_CLR_RED)
				{ // on the left
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left left
						s->child[TREE_DIR_LEFT]->color = s->color;
						s->color = p->color;
						alloc_tree_rotate(p, TREE_DIR_RIGHT);
					}
					else
					{ // right left
						s->child[TREE_DIR_LEFT]->color = p->color;
						alloc_tree_rotate(s, TREE_DIR_RIGHT);
						alloc_tree_rotate(p, TREE_DIR_LEFT);
					}
				}
				else
				{ // on the right
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left right
						s->child[TREE_DIR_RIGHT]->color = p->color;
						alloc_tree_rotate(s, TREE_DIR_LEFT);
						alloc_tree_rotate(p, TREE_DIR_RIGHT);
					}
					else
					{ // right right
						s->child[TREE_DIR_RIGHT]->color = s->color;
						s->color = p->color;
						alloc_tree_rotate(p, TREE_DIR_LEFT);
					}
				}
				p->color = TREE_CLR_BLACK;
			}
			else
			{ // 2 black children
				s->color = TREE_CLR_RED;
				if(p->color == TREE_CLR_BLACK)
					alloc_tree_delete_fixbb(n);
				else
					p->color = TREE_CLR_BLACK;
			}
		}
	}
}
void alloc_tree_delete(node *n)
{
	while(n)
	{
		node* u = alloc_tree_delete_replacement(n);
		int un_is_black = (!u || u->color == TREE_CLR_BLACK) && n->color == TREE_CLR_BLACK;

		node* p = n->parent;

		if(!u){
			if(n == alloc_tree.root)
				alloc_tree.root = NULL;
			else{
				if(un_is_black)
					alloc_tree_delete_fixbb(n);
				else
					if(TREE_GET_SIBLING(n))
						TREE_GET_SIBLING(n)->color = TREE_CLR_RED;

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
				n->addr = u->addr; n->size = u->size;
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
					u->color = TREE_CLR_BLACK;
				kfree(n);
			}
			return;
		}
		// swap u and n
		node tmp;
		tmp.size = n->size; tmp.addr = n->addr;
		n->size = u->size; n->addr = u->addr;
		u->size = tmp.size; u->addr = tmp.addr;
		n = u;
	}
}

/* DFS by size */
node* alloc_tree_find_first_fit_r(node* n, uint64_t size)
{
	if(n->size >= size)
		return n;

	if(n->child[TREE_DIR_LEFT])
	{ node* nn = alloc_tree_find_first_fit_r(n->child[TREE_DIR_LEFT], size); if(nn) return nn; }
	if(n->child[TREE_DIR_RIGHT])
	{ node* nn = alloc_tree_find_first_fit_r(n->child[TREE_DIR_RIGHT], size); if(nn) return nn; }

	return NULL;
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
	return NULL;
}
/* BT traversal by addr, but the criteria is addr + size interval lying in free space */
node* alloc_tree_find_containing(void* addr, uint64_t size)
{
	node* cur = alloc_tree.root;
	while(cur){
		if(addr >= cur->addr && addr + size <= cur->addr + cur->size)
			return cur;
		cur = (addr < cur->addr) ? cur->child[TREE_DIR_LEFT] : cur->child[TREE_DIR_RIGHT];
	}
	return NULL;
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
	uart_printf("%c %p $ %p\r\n", n->color == TREE_CLR_BLACK ? 'B' : 'R', n->addr, (void*)n->size);

	alloc_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	alloc_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}
