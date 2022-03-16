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
	void* address;
	uint64_t size;

	node *child[2];
	node *parent;
	uint8_t color;
};

static struct {
	node* root;
} alloc_tree;


void alloc_tree_insert(node* n);
void alloc_tree_insertp(node* n, node* p, int dir);
void alloc_tree_delete(node *n);

#define alloc_tree_find_first_fit(size) alloc_tree_find_first_fit_r(alloc_tree.root, size)
node* alloc_tree_find_first_fit_r(node* n, uint64_t size);
node* alloc_tree_find(void* address);

node* alloc_tree_rotate(node* p, int dir);

#define alloc_tree_print() alloc_tree_print_r(alloc_tree.root, 0)
void alloc_tree_print_r(node* n, unsigned depth);


// Public interface

void allocator_init(uint64_t memory_limit)
{
	alloc_tree.root = kmalloc(sizeof(node));
	alloc_tree.root->address = (void*)0;
	alloc_tree.root->size = memory_limit;
	alloc_tree.root->child[0] = alloc_tree.root->child[1] = alloc_tree.root->parent = NULL;
	alloc_tree.root->color = TREE_CLR_BLACK;

	/*uart_printf("RB tree insert test:\r\n");
	alloc_tree_print();

	void* raddr[10] = {(void*)0x182131, (void*)0x64812, (void*)0x568192, (void*)0x581921, (void*)0x4819185, (void*)0x913852, (void*)0x1288521, (void*)0x53818, (void*)0x543811, (void*)0x384821};
	uint64_t rsize[10] = {678381, 195821, 21948, 218412, 64831, 31852, 68832, 128543, 321818, 538181};
	node* nodes[10];

	for(unsigned i = 0; i < 10; ++i)
	{
		nodes[i] = kmalloc(sizeof(node));
		nodes[i]->address = raddr[i];
		nodes[i]->size = rsize[i];
		alloc_tree_insert(nodes[i]);
	}

	uart_printf("-------------insert-------------\r\n");
	alloc_tree_print();

	for(int i = 9; i >= 0; --i){
		node* todel = alloc_tree_find(raddr[i]);
		uart_printf("-------------delete %p-------------\r\n", raddr[i]);
		alloc_tree_delete(todel);
		alloc_tree_print();
	}*/
}


void* allocator_give(uint64_t size)
{
	return NULL;
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
		int dir = n->address < root->address ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		if(!root->child[dir]){
			root->child[dir] = n;
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
				n->address = u->address; n->size = u->size;
				n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
				kfree(u);
			}
			else
			{ // delete n from the tree and move u up
				p->child[TREE_DIR_CHILD(n)] = u;
				kfree(n);
				u->parent = p;
				if(un_is_black)
					alloc_tree_delete_fixbb(n);
				else
					u->color = TREE_CLR_BLACK;
			}
			return;
		}
		// swap u and n
		node tmp;
		tmp.size = n->size; tmp.address = n->address;
		n->size = u->size; n->address = u->address;
		u->size = tmp.size; u->address = tmp.address;
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
/* BT traversal by address */
node* alloc_tree_find(void* address)
{
	node* cur = alloc_tree.root;
	while(cur){
		if(cur->address == address)
			return cur;
		cur = (address < cur->address) ? cur->child[TREE_DIR_LEFT] : cur->child[TREE_DIR_RIGHT];
	}
	return NULL;
}

// RB tree helper functions

node* alloc_tree_rotate(node* p, int dir)
{
	node* g = p->parent;
	node* s = p->child[1 - dir];
	node* c = s->child[dir];

	p->child[1 - dir] = c;
	if(c) c->parent = p;
	s->child[dir] = p;
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
	uart_printf("%c %p $ %lu\r\n", n->color == TREE_CLR_BLACK ? 'B' : 'R', n->address, n->size);

	alloc_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	alloc_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}
